#pragma once

#include "stdio.h"
#include "string"
#include "format"
#include "iostream"
#include "libssh2.h"
#include "libssh2_sftp.h"
#include "sys/socket.h"
#include "unistd.h"
#include "netinet/in.h"
#include "arpa/inet.h"
#include <fcntl.h>
#include <filesystem>
#include "stdint.h"


class sftpController
{

public:
    sftpController();
    ~sftpController();
    sftpController(const sftpController &) = delete;
    sftpController &operator=(const sftpController &) = delete;


    bool connect_sftp(std::string_view addr,std::string_view username,std::string_view password);
    bool is_connected();
    void disconnect_sftp();

    bool transfer(std::string_view sourcePath, std::string_view destinationPath);
    bool transfer_pending_images(const std::filesystem::path &imageDirectory,
                                 const std::filesystem::path &remoteDirectory);

private:
    struct logLevel
    {
        static constexpr std::string_view info = "INFO";
        static constexpr std::string_view warn = "WARN";
        static constexpr std::string_view err = "ERR";
    };
    void logging(std::string_view loggingLevel, std::string msg);
    inline static const std::string logTag = "sftpController";


    uint32_t hostaddr_;
    libssh2_socket_t sock_ = LIBSSH2_INVALID_SOCKET;
    struct sockaddr_in sin_{};
    LIBSSH2_SESSION *session_ = nullptr;
    LIBSSH2_SFTP *sftp_session_ = nullptr;
    LIBSSH2_SFTP_HANDLE *sftp_handle_ = nullptr;
    const char *fingerPrint_;

    std::string addr_;
    std::string username_;
    std::string password_;

    bool libssh2_init_status_ = false;
    bool connected_ = false;
};