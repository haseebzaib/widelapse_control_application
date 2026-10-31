# Requires a C++20 compiler, libssh2 development files, and Python paramiko.
# Uses a local test server; no production server or camera is contacted.
import pathlib, socket, subprocess, tempfile, threading
import paramiko

root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='widelapse-sftp-') as tmp:
    base = pathlib.Path(tmp)
    images = base / 'images'
    remote = base / 'remote'
    images.mkdir()
    remote.mkdir()
    names = ['image_2026-10-01_09-00-00.jpg', 'image_2026-10-02_09-00-00.jpg', 'image_2026-10-03_09-00-00.jpg']
    for i, name in enumerate(names):
        (images / name).write_bytes(bytes([i + 1]) * 250000)
    (images / 'unfinished.jpg.part').write_bytes(b'partial')
    (images / 'already.jpg').write_bytes(b'sent')
    (images / 'already.jpg.sent').write_text('sent\n')
    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    listener.listen()
    port = listener.getsockname()[1]
    attempts = []
    key = paramiko.RSAKey.generate(2048)
    class Auth(paramiko.ServerInterface):
        def check_auth_password(self, user, password):
            return paramiko.AUTH_SUCCESSFUL
        def check_channel_request(self, kind, channel):
            return paramiko.OPEN_SUCCEEDED
    class Files(paramiko.SFTPServerInterface):
        failed = False
        def open(self, path, flags, attr):
            name = pathlib.Path(path).name
            attempts.append(name)
            if name == names[1] and not Files.failed:
                Files.failed = True
                return paramiko.SFTP_PERMISSION_DENIED
            handle = paramiko.SFTPHandle(flags)
            handle.writefile = open(remote / name, 'wb')
            return handle
    def serve():
        while True:
            try:
                connection, _ = listener.accept()
            except OSError:
                return
            transport = paramiko.Transport(connection)
            transport.add_server_key(key)
            transport.set_subsystem_handler('sftp', paramiko.SFTPServer, Files)
            transport.start_server(server=Auth())
            while transport.is_active():
                threading.Event().wait(.05)
    threading.Thread(target=serve, daemon=True).start()
    driver = base / 'check.cpp'
    driver.write_text('''
#include "sftpController/sftpController.hpp"
#include <stdexcept>
extern "C" int __real_connect(int, const sockaddr *, socklen_t);
extern "C" int __wrap_connect(int fd, const sockaddr *address, socklen_t length) {
    auto local = *reinterpret_cast<const sockaddr_in *>(address);
    local.sin_port = htons(TEST_PORT);
    return __real_connect(fd, reinterpret_cast<const sockaddr *>(&local), length);
}
int main(int argc, char **argv) {
    sftpController sftp;
    if (!sftp.connect_sftp("127.0.0.1", "test", "test"))
        throw std::runtime_error("test connect failed");
    if (sftp.transfer_pending_images(argv[1], "/remote"))
        throw std::runtime_error("expected first batch failure");
    auto first = std::filesystem::path(argv[1]) / "image_2026-10-01_09-00-00.jpg.sent";
    auto second = std::filesystem::path(argv[1]) / "image_2026-10-02_09-00-00.jpg.sent";
    if (!std::filesystem::exists(first) || std::filesystem::exists(second))
        throw std::runtime_error("incorrect failure tracking");
    if (!sftp.transfer_pending_images(argv[1], "/remote"))
        throw std::runtime_error("retry failed");
    if (!sftp.transfer_pending_images(argv[1], "/remote"))
        throw std::runtime_error("repeat scan failed");
}
''')
    binary = base / 'check'
    subprocess.run(['c++', '-std=c++20', f'-DTEST_PORT={port}', '-I' + str(root), str(driver), str(root / 'sftpController/sftpController.cpp'), '-lssh2', '-Wl,--wrap=connect', '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(images)], check=True, timeout=30)
    assert attempts == [names[0], names[1], names[1], names[2]], attempts
    for name in names:
        assert (images / name).read_bytes() == (remote / name).read_bytes()
        assert (images / (name + '.sent')).exists()
    listener.close()
    print('SFTP checks passed: oldest-first order, retry/reconnect, exact contents, .part exclusion, persistent markers, no duplicate upload')
