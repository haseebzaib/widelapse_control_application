#include "cameraController/cameraController.hpp"
#include <iostream>
#include <string>

int main()
{
    cameraController camera;

    if (!camera.connect())
    {
        std::cerr << "Camera connection failed\n";
        return 1;
    }

    std::string command;

    while (std::getline(std::cin, command))
    {
        if (command == "capture")
        {
            if (camera.capture_image("/home/hsb/Desktop/Freelance_Projects/Zero_Axis/widelapse/widelapse_control_application/build/images/image.jpg"))
                std::cout << "Image saved\n";
            else
                std::cout << "Capture failed\n";
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