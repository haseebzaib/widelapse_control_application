#include "sftpController.hpp"

#include <algorithm>
#include <fstream>
#include <vector>

sftpController::sftpController()
{
    connected_ = false;
    libssh2_init_status_ = false;

    int rt = libssh2_init(0);
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
    disconnect_sftp();
    if (libssh2_init_status_)
        libssh2_exit();
}

bool sftpController::connect_sftp(std::string_view addr, std::string_view username, std::string_view password)
{
    int rt = 1;

    addr_ = addr;
    username_ = username;
    password_ = password;

    if (libssh2_init_status_ == false)
    {

        int rt = libssh2_init(0);
        if (rt == 0)
        {
            libssh2_init_status_ = true;
        }
        else
        {
            logging(logLevel::err, "LIBSSH2 initialization failed");
            return false;
        }
    }
    disconnect_sftp();

    hostaddr_ = inet_addr(addr_.c_str());

    sock_ = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_ == LIBSSH2_INVALID_SOCKET)
    {
        logging(logLevel::err, "failed to create socket");
        disconnect_sftp();
        return false;
    }

    sin_.sin_family = AF_INET;
    sin_.sin_port = htons(22);
    sin_.sin_addr.s_addr = hostaddr_;

    logging(logLevel::info, std::format("hostaddr : {}", hostaddr_));

    if (connect(sock_, (struct sockaddr *)&sin_, sizeof(struct sockaddr_in)))
    {
        logging(logLevel::err, "failed to connect socket");
        disconnect_sftp();
        return false;
    }

    logging(logLevel::info, "sftp server connected");

    session_ = libssh2_session_init();

    if (!session_)
    {
        logging(logLevel::err, "failed to create session");
        disconnect_sftp();
        return false;
    }

    libssh2_session_set_blocking(session_, 1);
    libssh2_session_set_timeout(session_, 10000);

    rt = libssh2_session_handshake(session_, sock_);

    if (rt)
    {
        logging(logLevel::err, std::format("failure establishing an ssh session: {}", rt));
        disconnect_sftp();
        return false;
    }

    fingerPrint_ = libssh2_hostkey_hash(session_, LIBSSH2_HOSTKEY_HASH_SHA256);
    // The fingerprint is binary data, not a null-terminated string.

    if (!fingerPrint_)
    {
        logging(logLevel::err, "fingerprint is null");
        disconnect_sftp();
        return false;
    }

    if (libssh2_userauth_password(session_, username_.c_str(), password_.c_str()))
    {
        logging(logLevel::info, "Authentication by password failed");
        disconnect_sftp();
        return false;
    }

    sftp_session_ = libssh2_sftp_init(session_);
    if (!sftp_session_)
    {
        logging(logLevel::info, "Unable to init SFTP server");
        disconnect_sftp();
        return false;
    }

    connected_ = true;

    return true;
}

bool sftpController::is_connected()
{
    return connected_;
}

void sftpController::disconnect_sftp()
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
        shutdown(sock_, 2);
        close(sock_);
        sock_ = LIBSSH2_INVALID_SOCKET;
    }

    connected_ = false;
}

bool sftpController::transfer(std::string_view sourcePath, std::string_view destinationPath)
{
    if (!connected_ && !connect_sftp(addr_, username_, password_))
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
        disconnect_sftp();
        return false;
    }

    bool success = true;
    char buffer[1024 * 100];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), local)) > 0)
    {
        size_t offset = 0;
        while (offset < count)
        {
            const auto written = libssh2_sftp_write(sftp_handle_, buffer + offset, count - offset);
            if (written <= 0)
            {
                success = false;
                break;
            }
            offset += static_cast<size_t>(written);
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
        disconnect_sftp();
    }
    return success;
}

bool sftpController::transfer_pending_images(const std::filesystem::path &imageDirectory,
                                              const std::filesystem::path &remoteDirectory)
{
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
    // Timestamped filenames sort from oldest to newest. Snapshot once per run.
    std::sort(pending.begin(), pending.end());
    for (const auto &path : pending)
    {
        if (!transfer(path.string(), (remoteDirectory / path.filename()).string()))
            return false; // Leave this and remaining images pending for the next run.

        std::ofstream marker(path.string() + ".sent");
        marker << "sent\n";
        marker.close();
        if (!marker)
        {
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