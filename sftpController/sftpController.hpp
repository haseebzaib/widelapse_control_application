#pragma once

#include "string"
#include "format"
#include "iostream"
#include "libssh2.h"
#include "libssh2_sftp.h"

class sftpController
{

public:
    sftpController();
    ~sftpController();
    sftpController(const sftpController &) = delete;
    sftpController &operator=(const sftpController &) = delete;

    bool connect();
    void disconnect();

    bool transfer(std::string sourcePath, std::string destinationPath);

private:
    struct logLevel
    {
        static constexpr std::string_view info = "INFO";
        static constexpr std::string_view warn = "WARN";
        static constexpr std::string_view err = "ERR";
    };
    void logging(std::string_view loggingLevel, std::string msg);
    inline static const std::string logTag = "sftpController";
    bool connected_ = false;
};