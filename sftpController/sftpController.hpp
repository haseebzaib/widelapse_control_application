#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <netinet/in.h>

class sftpController
{
public:
    sftpController();
    ~sftpController();

    sftpController(const sftpController &) = delete;
    sftpController &operator=(const sftpController &) = delete;

    struct imageDirectoryInfo
    {
        std::uint32_t totalImages = 0; // Shots taken
        std::uint32_t totalImagesTransfered = 0;
        std::uint32_t totalImagesFailed = 0;
    };

    struct sftpServerInfo
    {
        std::string addr;
        int port = 0;
        std::string username;
        std::string password;
        std::string remoteDirectoryPath;
    };

    bool connect_sftp();
    bool is_connected();
    void disconnect_sftp();

    void update_server_info(std::string_view addr,
                            int port,
                            std::string_view username,
                            std::string_view password,
                            std::string_view directory);

    sftpServerInfo get_server_info();

    bool transfer(std::string_view sourcePath, std::string_view destinationPath);
    imageDirectoryInfo image_direcotry_info(); // Existing public name preserved
    bool transfer_pending_images(const std::filesystem::path &imageDirectory);

private:
    struct logLevel
    {
        static constexpr std::string_view info = "INFO";
        static constexpr std::string_view warn = "WARN";
        static constexpr std::string_view err = "ERR";
    };

    void logging(std::string_view loggingLevel, std::string msg);
    inline static const std::string logTag = "sftpController";

    // Must be called ONLY while holding sftpServerInfoMutex_.
    bool connect_sftp_unlocked();
    void disconnect_sftp_unlocked();
    bool transfer_unlocked(std::string_view sourcePath,
                           std::string_view destinationPath);

    imageDirectoryInfo imageDirectoryInfo_{};

    std::uint32_t hostaddr_ = 0;
    libssh2_socket_t sock_ = LIBSSH2_INVALID_SOCKET;
    struct sockaddr_in sin_{};
    LIBSSH2_SESSION *session_ = nullptr;
    LIBSSH2_SFTP *sftp_session_ = nullptr;
    LIBSSH2_SFTP_HANDLE *sftp_handle_ = nullptr;
    const char *fingerPrint_ = nullptr;

    sftpServerInfo sftpServerInfo_{};

    // Protects configuration, all session/socket/handle operations,
    // connected_, and imageDirectoryInfo_ counters.
    mutable std::mutex sftpServerInfoMutex_;

    bool libssh2_init_status_ = false;
    bool connected_ = false;
};
