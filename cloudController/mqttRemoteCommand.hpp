#pragma once

#include "mqttController.hpp"
#include "unordered_map"
#include "functional"
#include "string"
#include <utils/nlohmann/json.hpp>

namespace cloudController
{


     namespace mqttRemoteCommand
    {
        

        




        std::unordered_map<std::string,std::function<void(const  nlohmann::json&)>> get_commands_callbacks();



    }


}

