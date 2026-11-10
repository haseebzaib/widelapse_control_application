#include "diagnostic.hpp"

namespace diagnostic
{
    // void loggingDiagnostic(std::string msg)
    // {
    //     std::cout << std::format("[{}] {}\n", "diagnostic", msg);
    // }

    void get_storage_info(const std::string &path, int &total, int &used, int &free)
    {
        std::error_code ec;
        auto info = std::filesystem::space(path, ec);

        if (ec)
        {
            // loggingDiagnostic(std::format("Unable to read storage: {}", ec.message()));

            total = used = free = 0;
            return;
        }

        constexpr uint64_t MB = 1024 * 1024;

        total = static_cast<int>(info.capacity / MB);
        used = static_cast<int>((info.capacity - info.free) / MB);
        free = static_cast<int>(info.available / MB);

        // loggingDiagnostic(std::format(
        //     "Storage Path: {} | Total: {} MiB | Used: {} MiB | Available: {} MiB",
        //     path, total, used, free));
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
                        // loggingDiagnostic(std::format(
                        //     "Cpu:{} Temp:{}", i, diagnostic.tempCpu[i]));
                    }
                }
                else
                {
                    while (streams[i] >> key >> value >> unit)
                    {
                        if (key == "MemTotal:")
                        {
                            diagnostic.ramTotal = value / 1024;
                            // loggingDiagnostic(std::format(
                            //     "ramTotal:{}", diagnostic.ramTotal));
                        }
                        else if (key == "MemFree:")
                        {
                            diagnostic.ramFree = value / 1024;
                            // loggingDiagnostic(std::format(
                            //     "ramFree:{}", diagnostic.ramFree));
                        }
                    }
                }

                streams[i].close();
            }
        }

        get_storage_info("/", diagnostic.memTotalRootfs, diagnostic.memUsedRootfs, diagnostic.memFreeRootfs);
        get_storage_info("/opt/widelapse", diagnostic.memTotalApplication, diagnostic.memUsedApplication, diagnostic.memFreeApplication);
    }

    nlohmann::json get_info_json()
    {
        std::ifstream streams[5];
        std::string files[5] = {"/sys/class/thermal/thermal_zone0/temp", "/sys/class/thermal/thermal_zone1/temp", "/sys/class/thermal/thermal_zone2/temp", "/sys/class/thermal/thermal_zone3/temp", "/proc/meminfo"};

        std::string key;
        int value;
        std::string unit;

        systemDiagnostic diagnostic;

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
                        // loggingDiagnostic(std::format(
                        //     "Cpu:{} Temp:{}", i, diagnostic.tempCpu[i]));
                    }
                }
                else
                {
                    while (streams[i] >> key >> value >> unit)
                    {
                        if (key == "MemTotal:")
                        {
                            diagnostic.ramTotal = value / 1024;
                            // loggingDiagnostic(std::format(
                            //     "ramTotal:{}", diagnostic.ramTotal));
                        }
                        else if (key == "MemFree:")
                        {
                            diagnostic.ramFree = value / 1024;
                            // loggingDiagnostic(std::format(
                            //     "ramFree:{}", diagnostic.ramFree));
                        }
                    }
                }

                streams[i].close();
            }
        }

        get_storage_info("/", diagnostic.memTotalRootfs, diagnostic.memUsedRootfs, diagnostic.memFreeRootfs);
        get_storage_info("/opt/widelapse", diagnostic.memTotalApplication, diagnostic.memUsedApplication, diagnostic.memFreeApplication);

        nlohmann::json result = {
            {"cpu", {{"temp0", diagnostic.tempCpu[0]}, {"temp1", diagnostic.tempCpu[1]}, {"temp2", diagnostic.tempCpu[2]}, {"temp3", diagnostic.tempCpu[3]}}},
            {"ram", {{"total", diagnostic.ramTotal}, {"free", diagnostic.ramFree}}},
            {"storage", {{"rootfs", {{"total", diagnostic.memTotalRootfs}, {"used", diagnostic.memUsedRootfs}, {"free", diagnostic.memFreeRootfs}}}, {"application", {{"total", diagnostic.memTotalApplication}, {"used", diagnostic.memUsedApplication}, {"free", diagnostic.memFreeApplication}}}}}};

        return result;
    }
}
