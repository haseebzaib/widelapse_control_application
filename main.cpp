#include "cameraController/cameraController.hpp"
#include "sftpController/sftpController.hpp"
#include "schedulingController/schedulingController.hpp"
#include <iostream>
#include <string>
#include "filesystem"
#include "chrono"
#include "ctime"
#include "format"

bool sftp_connected = false;


schedulingController cameraScheduller("cameraScheduller");


std::string get_image_name_with_timestamp()
{
    std::chrono::time_point now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    localtime_r(&time, &localTime);

    return std::format("image_{:04}-{:02}-{:02}_{:02}-{:02}-{:02}.jpg",
                       localTime.tm_year + 1900,
                       localTime.tm_mon + 1,
                       localTime.tm_mday,
                       localTime.tm_hour,
                       localTime.tm_min,
                       localTime.tm_sec);
}



int main()
{
    cameraController camera;
    sftpController sftpServer;

    sftp_connected = sftpServer.connect_sftp("94.130.57.90", "sftpuser", "P@ssword.com@2k25*");

    if (!sftp_connected)
    {
        std::cout << std::format("SFTP failed to connect\n");
    }
    if (!camera.connect())
    {
        std::cout << "Camera connection failed\n";
    }

    std::string command;

    while (std::getline(std::cin, command))
    {

        if (command == "capture")
        {

            std::string imageName = get_image_name_with_timestamp();
            std::filesystem::path basePath = "/opt/widelapse/images";
            std::filesystem::path imagePath = basePath / imageName;

            std::string sftpServerPath = std::format("/media/newc2ae86228-projcde50db9-came935a3401/{}", imageName);

            if (camera.capture_image(imagePath))
            {
                std::cout
                    << "Image saved\n";
                std::string str = imagePath.string();
                if(!sftpServer.transfer(str, sftpServerPath))
                {
                   std::cout << "Cimage failed to send to sftp server\n"; 
                }
                else
                {
                     std::cout << "image sftp success\n"; 
                }
            }
            else
            {
                std::cout << "Capture failed\n";
            }
        }
        else if (command == "summary")
        {
            std::cout << camera.get_summary() << '\n';
        }
        else if (command == "exit")
        {
            break;
        }
    }

    return 0;
}