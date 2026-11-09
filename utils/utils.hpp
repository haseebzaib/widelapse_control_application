#pragma once
#include "iostream"
#include "fstream"
#include <filesystem>
#include "stdio.h"
#include "string"
#include "format"




namespace utils
{


    std::string get_ethernet_mac()
    {
        std::ifstream file("/sys/class/net/end0/address");
        std::string mac;

        if(!std::getline(file,mac))
        {
            return {};
        }

        return mac;
    }


}