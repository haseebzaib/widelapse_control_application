#include "sftpController.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>

sftpController::sftpController()
{
    const int rt = libssh2_init(0);
    if (rt == 0)
    {
        libssh2_init_status_ = true;
    }
    else
    {
        logging(logLevel::err, "LIBSSH2 initialization failed");
    }
}

sftpController::~sftpController()
{
    // Callers must stop/join other threads using this object before destruction.
    disconnect_sftp();

    if (libssh2_init_status_)
        libssh2_exit();
}

bool sftpController::connect_sftp()
{
    std::lock_guard lock(sftpServerInfoMutex_);
    return connect_sftp_unlocked();
}

// Caller must hold sftpServerInfoMutex_.
bool sftpController::connect_sftp_unlocked()
{
    if (!libssh2_init_status_)
    {
        if (libssh2_init(0) != 0)
        {
            logging(logLevel::err, "LIBSSH2 initialization failed");
            return false;
        }
        libssh2_init_status_ = true;
    }

    // Safe: this helper does not try to acquire the mutex again.
    disconnect_sftp_unlocked();

    if (sftpServerInfo_.port <= 0 || sftpServerInfo_.port > 65535)
    {
        logging(logLevel::err, "Invalid SFTP port");
        return false;
    }

    sin_ = {};
    sin_.sin_family = AF_INET;
    sin_.sin_port = htons(static_cast<std::uint16_t>(sftpServerInfo_.port));
    if (inet_pton(AF_INET, sftpServerInfo_.addr.c_str(), &sin_.sin_addr) != 1)
    {
        logging(logLevel::err, "Invalid SFTP IPv4 address: " + sftpServerInfo_.addr);
        return false;
    }
    hostaddr_ = sin_.sin_addr.s_addr;

    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == LIBSSH2_INVALID_SOCKET)
    {
        logging(logLevel::err, "Failed to create socket");
        disconnect_sftp_unlocked();
        return false;
    }

    logging(logLevel::info, "Connecting to " + sftpServerInfo_.addr +
                                ":" + std::to_string(sftpServerInfo_.port));

    if (::connect(sock_, reinterpret_cast<struct sockaddr *>(&sin_),
                  sizeof(sin_)) != 0)
    {
        logging(logLevel::err, "Failed to connect socket");
        disconnect_sftp_unlocked();
        return false;
    }

    session_ = libssh2_session_init();
    if (!session_)
    {
        logging(logLevel::err, "Failed to create SSH session");
        disconnect_sftp_unlocked();
        return false;
    }

    libssh2_session_set_blocking(session_, 1);
    libssh2_session_set_timeout(session_, 10000);

    const int handshakeResult = libssh2_session_handshake(session_, sock_);
    if (handshakeResult != 0)
    {
        logging(logLevel::err,
                std::format("Failed to establish SSH session: {}", handshakeResult));
        disconnect_sftp_unlocked();
        return false;
    }

    fingerPrint_ = libssh2_hostkey_hash(session_, LIBSSH2_HOSTKEY_HASH_SHA256);
    if (!fingerPrint_)
    {
        logging(logLevel::err, "Unable to obtain server fingerprint");
        disconnect_sftp_unlocked();
        return false;
    }
    // IMPORTANT: Obtaining the fingerprint alone does not verify server identity.

    if (libssh2_userauth_password(session_,
                                  sftpServerInfo_.username.c_str(),
                                  sftpServerInfo_.password.c_str()) != 0)
    {
        logging(logLevel::err, "SFTP password authentication failed");
        disconnect_sftp_unlocked();
        return false;
    }

    sftp_session_ = libssh2_sftp_init(session_);
    if (!sftp_session_)
    {
        logging(logLevel::err, "Unable to initialize SFTP session");
        disconnect_sftp_unlocked();
        return false;
    }

    connected_ = true;
    logging(logLevel::info, "SFTP session connected");
    return true;
}

bool sftpController::is_connected()
{
    std::lock_guard lock(sftpServerInfoMutex_);
    return connected_;
}

void sftpController::disconnect_sftp()
{
    std::lock_guard lock(sftpServerInfoMutex_);
    disconnect_sftp_unlocked();
}

// Caller must hold sftpServerInfoMutex_.
void sftpController::disconnect_sftp_unlocked()
{
    if (sftp_handle_)
    {
        libssh2_sftp_close(sftp_handle_);
        sftp_handle_ = nullptr;
    }

    if (sftp_session_)
    {
        libssh2_sftp_shutdown(sftp_session_);
        sftp_session_ = nullptr;
    }

    if (session_)
    {
        libssh2_session_disconnect(session_, "Normal Shutdown");
        libssh2_session_free(session_);
        session_ = nullptr;
    }

    if (sock_ != LIBSSH2_INVALID_SOCKET)
    {
        shutdown(sock_, SHUT_RDWR);
        close(sock_);
        sock_ = LIBSSH2_INVALID_SOCKET;
    }

    fingerPrint_ = nullptr;
    hostaddr_ = 0;
    connected_ = false;
}

void sftpController::update_server_info(std::string_view addr,
                                        int port,
                                        std::string_view username,
                                        std::string_view password,
                                        std::string_view directory)
{
    // Construct the new configuration before touching the active session.
    sftpServerInfo updated{
        std::string(addr), port, std::string(username),
        std::string(password), std::string(directory)};

    std::lock_guard lock(sftpServerInfoMutex_);

    // Disconnect the old server and replace the configuration atomically
    // with respect to connect/transfer/disconnect calls on this instance.
    disconnect_sftp_unlocked();
    std::swap(sftpServerInfo_, updated);

    logging(logLevel::info, "SFTP server information updated; previous session disconnected");
}

sftpController::sftpServerInfo sftpController::get_server_info()
{
    std::lock_guard lock(sftpServerInfoMutex_);
    return sftpServerInfo_;
}

bool sftpController::transfer(std::string_view sourcePath,
                              std::string_view destinationPath)
{
    std::lock_guard lock(sftpServerInfoMutex_);
    return transfer_unlocked(sourcePath, destinationPath);
}

// Caller must hold sftpServerInfoMutex_.
bool sftpController::transfer_unlocked(std::string_view sourcePath,
                                       std::string_view destinationPath)
{
    if (!connected_ && !connect_sftp_unlocked())
        return false;

    const std::string source(sourcePath);
    const std::string destination(destinationPath);

    FILE *local = fopen(source.c_str(), "rb");
    if (!local)
    {
        logging(logLevel::err, "Cannot open local file " + source);
        return false;
    }

    sftp_handle_ = libssh2_sftp_open(sftp_session_, destination.c_str(),
                                     LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
                                     0644);
    if (!sftp_handle_)
    {
        logging(logLevel::err, "Unable to open remote file " + destination);
        fclose(local);
        disconnect_sftp_unlocked();
        return false;
    }

    bool success = true;
    char buffer[1024 * 100];
    std::size_t count;

    while ((count = fread(buffer, 1, sizeof(buffer), local)) > 0)
    {
        std::size_t offset = 0;
        while (offset < count)
        {
            const auto written = libssh2_sftp_write(sftp_handle_, buffer + offset,
                                                     count - offset);
            if (written <= 0)
            {
                success = false;
                break;
            }
            offset += static_cast<std::size_t>(written);
        }
        if (!success)
            break;
    }

    success = success && !ferror(local);
    fclose(local);

    const int closeResult = libssh2_sftp_close(sftp_handle_);
    sftp_handle_ = nullptr;
    success = success && closeResult == 0;

    if (!success)
    {
        logging(logLevel::err, "File transfer failed: " + source);
        disconnect_sftp_unlocked();
    }

    return success;
}

sftpController::imageDirectoryInfo sftpController::image_direcotry_info()
{
    std::lock_guard lock(sftpServerInfoMutex_);

    imageDirectoryInfo_.totalImages = 0;
    imageDirectoryInfo_.totalImagesTransfered = 0;

    const std::filesystem::path imageDirectory("/opt/widelapse/images");
    if (std::filesystem::exists(imageDirectory))
    {
        for (const auto &entry : std::filesystem::directory_iterator(imageDirectory))
        {
            const auto &path = entry.path();
            if (entry.is_regular_file() && path.extension() == ".jpg")
            {
                ++imageDirectoryInfo_.totalImages;
                if (std::filesystem::exists(path.string() + ".sent"))
                    ++imageDirectoryInfo_.totalImagesTransfered;
            }
        }
    }

    // totalImagesFailed is cumulative and is intentionally not reset here.
    return imageDirectoryInfo_;
}

bool sftpController::transfer_pending_images(const std::filesystem::path &imageDirectory)
{
    if (!std::filesystem::exists(imageDirectory) ||
        !std::filesystem::is_directory(imageDirectory))
    {
        logging(logLevel::err, "Image directory does not exist: " + imageDirectory.string());
        return false;
    }

    std::vector<std::filesystem::path> pending;
    for (const auto &entry : std::filesystem::directory_iterator(imageDirectory))
    {
        const auto &path = entry.path();
        if (entry.is_regular_file() && path.extension() == ".jpg" &&
            !std::filesystem::exists(path.string() + ".sent"))
        {
            pending.push_back(path);
        }
    }

    // Timestamped filenames sort from oldest to newest.
    std::sort(pending.begin(), pending.end());

    for (const auto &path : pending)
    {
        std::lock_guard lock(sftpServerInfoMutex_);

        // Another call to transfer_pending_images() might have sent it already.
        // Recheck under the mutex before starting the transfer.
        if (!std::filesystem::is_regular_file(path) ||
            std::filesystem::exists(path.string() + ".sent"))
        {
            continue;
        }

        const auto destination =
            std::filesystem::path(sftpServerInfo_.remoteDirectoryPath) / path.filename();

        if (!transfer_unlocked(path.string(), destination.string()))
        {
            ++imageDirectoryInfo_.totalImagesFailed;
            return false; // Keep the image pending for the next attempt.
        }

        // Keep the lock until the marker is written, so another transfer call
        // cannot upload the same image before its .sent marker appears.
        std::ofstream marker(path.string() + ".sent");
        marker << "sent\n";
        marker.close();
        if (!marker)
        {
            ++imageDirectoryInfo_.totalImagesFailed;
            logging(logLevel::err, "Cannot record upload for " + path.string());
            std::error_code error;
            std::filesystem::remove(path.string() + ".sent", error);
            return false;
        }

        logging(logLevel::info, "Image sent: " + path.filename().string());
    }

    return true;
}

void sftpController::logging(std::string_view loggingLevel, std::string msg)
{
    std::cout << std::format("[{}][{}] {}\n", loggingLevel, logTag, msg);
}
