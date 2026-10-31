#include "cameraController/cameraController.hpp"
#include "sftpController/sftpController.hpp"
#include "schedulingController/schedulingController.hpp"
#include <chrono>
#include <ctime>
#include <filesystem>
#include <format>
#include <iostream>
#include <mutex>
#include <string>

std::string get_image_name_with_timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    localtime_r(&time, &localTime);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000;

    return std::format("image_{:04}-{:02}-{:02}_{:02}-{:02}-{:02}_{:03}.jpg",
                       localTime.tm_year + 1900, localTime.tm_mon + 1,
                       localTime.tm_mday, localTime.tm_hour,
                       localTime.tm_min, localTime.tm_sec, milliseconds);
}

int main()
{
    using namespace std::chrono;
    cameraController camera;
    sftpController sftpServer;
    std::mutex cameraMutex;
    const std::filesystem::path imageDirectory = "/opt/widelapse/images";
    const std::filesystem::path remoteDirectory = "/media/newc2ae86228-projcde50db9-came935a3401";
    std::filesystem::create_directories(imageDirectory);

    const auto capture = [&]
    {
        std::lock_guard lock(cameraMutex);
        const auto imagePath = imageDirectory / get_image_name_with_timestamp();
        // Publish the .jpg only after capture finishes so transfer cannot read it early.
        const std::filesystem::path temporaryPath = imagePath.string() + ".part";
        if (camera.capture_image(temporaryPath))
        {
            std::filesystem::rename(temporaryPath, imagePath);
            std::cout << "Image saved: " << imagePath << '\n';
        }
        else
        {
            std::filesystem::remove(temporaryPath);
            std::cout << "Capture failed\n";
        }
    };

    const auto transfer = [&]
    {
        if (!sftpServer.is_connected() && !sftpServer.connect_sftp("94.130.57.90", "sftpuser", "P@ssword.com@2k25*"))
        {
            std::cout << "SFTP connection failed; retrying next transfer interval\n";
            return;
        }
        sftpServer.transfer_pending_images(imageDirectory, remoteDirectory);
    };

    // Declared last: both workers stop before their captured resources are destroyed.
    schedulingController cameraScheduler("cameraScheduler");
    schedulingController transferScheduler("transferScheduler");

    // Example camera window: Monday and Tuesday, 09:00 <= time < 17:00.
    cameraScheduler.add_profile({"camera", {1, 2}, hours(9), hours(17), minutes(5), capture});
    transferScheduler.add_profile({"transfer", {}, hours(0), hours(24), minutes(10), transfer});

    std::cout << "Schedules running. Commands: capture, summary, exit\n";
    std::string command;
    while (std::getline(std::cin, command))
    {
        if (command == "capture")
        {
            capture();
        }
        else if (command == "summary")
        {
            std::lock_guard lock(cameraMutex);
            if (camera.connect())
                std::cout << camera.get_summary() << '\n';
        }
        else if (command == "exit")
        {
            break;
        }
    }
    return 0;
}
