#include "sftpController.hpp"

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
}

bool sftpController::connect_sftp(std::string_view addr, std::string_view username, std::string_view password)
{
    int rt = 1;
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

    hostaddr_ = inet_addr(addr.data());

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

    rt = libssh2_session_handshake(session_, sock_);

    if (rt)
    {
        logging(logLevel::err, std::format("failure establishing an ssh session: {}", rt));
        disconnect_sftp();
        return false;
    }

    fingerPrint_ = libssh2_hostkey_hash(session_, LIBSSH2_HOSTKEY_HASH_SHA256);
    logging(logLevel::info, std::format("fingerprint: {}", fingerPrint_));

    if (!fingerPrint_)
    {
        logging(logLevel::err, "fingerprint is null");
        disconnect_sftp();
        return false;
    }

    if (libssh2_userauth_password(session_, username.data(), password.data()))
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

    return rt != 0;
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
    }

    if (sftp_session_)
    {
        libssh2_sftp_shutdown(sftp_session_);
    }

    if (session_)
    {
        libssh2_session_disconnect(session_, "Normal Shutdown");
        libssh2_session_free(session_);
    }
    if (sock_ != LIBSSH2_INVALID_SOCKET)
    {
        shutdown(sock_, 2);
        LIBSSH2_SOCKET_CLOSE(sock_);
    }
    if (!libssh2_init_status_)
    {
        libssh2_exit();
    }
    libssh2_init_status_ = false;
    connected_ = false;
}

bool sftpController::transfer(std::string_view sourcePath, std::string_view destinationPath)
{
    int rt = 1;

    FILE *local;
    char mem[1024 * 100];
    size_t nread;
    ssize_t nwritten;
    char *ptr;

    if (libssh2_init_status_ == false || connected_ == false)
    {
        return false;
    }

    sftp_handle_ = libssh2_sftp_open(sftp_session_, destinationPath.data(),
                                     LIBSSH2_FXF_WRITE |
                                         LIBSSH2_FXF_CREAT |
                                         LIBSSH2_FXF_TRUNC,
                                     LIBSSH2_SFTP_S_IRUSR |
                                         LIBSSH2_SFTP_S_IWUSR |
                                         LIBSSH2_SFTP_S_IRGRP |
                                         LIBSSH2_SFTP_S_IROTH);

    if (!sftp_handle_)
    {
        logging(logLevel::err, std::format("Unable to open file with SFTP: {}",
                                           libssh2_sftp_last_error(sftp_session_)));
        disconnect_sftp();
        return false;
    }

    local = fopen(sourcePath.data(), "rb");
    if (!local)
    {
        logging(logLevel::err, std::format("Cannot open local file {}", sourcePath.data()));
        return false;
    }

    do
    {
        nread = fread(mem, 1, sizeof(mem), local);
        if (nread <= 0)
            break; /* end of file */
        ptr = mem;

        do
        {
            /* write data in a loop until we block */
            nwritten = libssh2_sftp_write(sftp_handle_, ptr, nread);
            if (nwritten < 0)
                break;
            ptr += nwritten;
            nread -= (size_t)nwritten;
        } while (nread);
    } while (nwritten > 0);

    if (sftp_handle_)
    {
        libssh2_sftp_close(sftp_handle_);
    }

    return rt != 0;
}

void sftpController::logging(std::string_view loggingLevel, std::string msg)
{
    std::cout << std::format("[{}][{}] {}\n", loggingLevel, logTag, msg);
}