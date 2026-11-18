#include "utils.hpp"
#include <cstdlib>
#include <fstream>
#include <filesystem>



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

    bool reload_network()
    {
        int status = std::system(
            "/usr/local/bin/widelapse-network --reload");

        if (status != 0)
        {
            // logging("Failed to reload network");
            return false;
        }

        // logging("Network reloaded successfully");

        return true;
    }
    bool save_json(const std::string &path, const nlohmann::json &data)
    {
        try {
            std::filesystem::path filePath(path);

            //create or overwrite the file
            std::ofstream file(path, std::ios::trunc);
            if(!file.is_open())
            {
                return false;
            }
            file << data.dump(4);
            file.close();
            return !file.fail();

        }
        catch (const std::exception &e) {
            std::cerr << e.what() << "\n";

            return false;
        }
    }
    nlohmann::json read_json(const std::string &path)
    {
        try {
            std::ifstream file(path);
            if(!file.is_open())
            {
              return nullptr;
            }
            nlohmann::json fileData = nlohmann::json::parse(file);
            file.close();
            return fileData;
        }
        catch(const std::exception &e) {
            std::cerr << e.what() << "\n";
            return nullptr;
        }
    }
}
