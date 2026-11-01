#pragma once
#include "iostream"
#include "fstream"
#include <filesystem>
#include "stdio.h"
#include "string"
#include "format"

namespace diagnostic
{

    struct systemDiagnostic
    {
        int tempCpu[4]; // 4 cores
        int ramTotal;   // MB
        int ramFree;    // MB
        int memTotalRootfs;
        int memUsedRootfs;
        int memFreeRootfs;
        int memTotalApplication;
        int memUsedApplication;
        int memFreeApplication;
    };

    void logging(std::string msg)
    {
        std::cout << std::format("[{}] {}\n", "diagnostic", msg);
    }

    void get_storage_info(const std::string &path, int &total, int &used, int &free)
    {
        std::error_code ec;
        auto info = std::filesystem::space(path, ec);

        if (ec)
        {
            logging(std::format("Unable to read storage: {}", ec.message()));

            total = used = free = 0;
            return;
        }

        constexpr uint64_t MB = 1024 * 1024;

        total = static_cast<int>(info.capacity / MB);
        used = static_cast<int>((info.capacity - info.free) / MB);
        free = static_cast<int>(info.available / MB);

        logging(std::format(
            "Storage Path: {} | Total: {} MiB | Used: {} MiB | Available: {} MiB",
            path, total, used, free));
    }

    void get_info(systemDiagnostic &diagnostic)
    {
        std::ifstream streams[5];
        std::string files[5] = {"/sys/class/thermal/thermal_zone0/temp", "/sys/class/thermal/thermal_zone1/temp", "/sys/class/thermal/thermal_zone2/temp", "/sys/class/thermal/thermal_zone3/temp", "/proc/meminfo"};

        std::string key;
        int value;
        std::string unit;

        for (int i = 0; i < 5; i++)
        {
            streams[i].open(files[i]);
            if (streams[i].is_open())
            {
                if (i < 4)
                {
                    if (streams[i] >> diagnostic.tempCpu[i])
                    {
                        diagnostic.tempCpu[i] = diagnostic.tempCpu[i] / 1000.0;
                        logging(std::format(
                            "Cpu:{} Temp:{}", i, diagnostic.tempCpu[i]));
                    }
                }
                else
                {
                    while (streams[i] >> key >> value >> unit)
                    {
                        if (key == "MemTotal:")
                        {
                            diagnostic.ramTotal = value / 1024;
                            logging(std::format(
                                "ramTotal:{}", diagnostic.ramTotal));
                        }
                        else if (key == "MemFree:")
                        {
                            diagnostic.ramFree = value / 1024;
                            logging(std::format(
                                "ramFree:{}", diagnostic.ramFree));
                        }
                    }
                }

                streams[i].close();
            }
        }

        get_storage_info("/",diagnostic.memTotalRootfs,diagnostic.memUsedRootfs,diagnostic.memFreeRootfs);
        get_storage_info("/opt/widelapse",diagnostic.memTotalApplication,diagnostic.memUsedApplication,diagnostic.memFreeApplication);
      
    }
};
