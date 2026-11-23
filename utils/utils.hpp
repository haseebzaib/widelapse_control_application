#pragma once
#include "iostream"
#include "fstream"
#include <filesystem>
#include "stdio.h"
#include "string"
#include "format"
#include <utils/nlohmann/json.hpp>
#include <stdexcept>

namespace utils
{

    std::string get_ethernet_mac();

    nlohmann::json getNetworkInfo();

    bool reload_network();
    nlohmann::json getWifiScan();
    bool save_json(const std::string &path,const nlohmann::json &data);
    nlohmann::json read_json(const std::string &path);
}
