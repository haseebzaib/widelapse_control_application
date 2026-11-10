#pragma once
#include "iostream"
#include "fstream"
#include <filesystem>
#include "stdio.h"
#include "string"
#include "format"
#include <utils/nlohmann/json.hpp>

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

    void get_storage_info(const std::string &path, int &total, int &used, int &free);

    void get_info(systemDiagnostic &diagnostic);

    nlohmann::json get_info_json();
}
