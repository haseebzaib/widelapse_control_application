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
}
