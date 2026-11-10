#include "utils.hpp"

namespace utils
{
    std::string get_ethernet_mac()
    {
        std::ifstream file("/sys/class/net/end0/address");
        std::string mac;

        if (!std::getline(file, mac))
        {
            return {};
        }

        return mac;
    }

    nlohmann::json getNetworkInfo()
    {
        FILE *pipe = popen(
            "/usr/local/bin/widelapse-network --network", "r");

        if (!pipe)
            throw std::runtime_error("Failed to execute script");

        std::string output;
        char buffer[4096];

        while (fgets(buffer, sizeof(buffer), pipe))
        {
            output += buffer;
        }

        int status = pclose(pipe);

        if (status != 0)
            throw std::runtime_error("Network script failed");

        return nlohmann::json::parse(output);
    }

}
