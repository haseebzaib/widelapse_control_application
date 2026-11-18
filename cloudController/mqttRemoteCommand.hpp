#pragma once

#include "mqttController.hpp"
#include "unordered_map"
#include "functional"
#include "string"
#include <utils/nlohmann/json.hpp>
#include "schedulingController/schedulingController.hpp"

namespace cloudController
{


     namespace mqttRemoteCommand
    {
        

        




        std::unordered_map<std::string,std::function<void(const  nlohmann::json&)>> get_commands_callbacks();

        // Call before MQTT starts. Connect existing workers and reload saved profiles.
        void init_scheduling(schedulingController &cameraScheduler,
                             schedulingController &transferScheduler,
                             std::function<void()> capture,
                             std::function<void()> transfer,
                             std::string path = "/opt/widelapse/configs/scheduling.json");



    }


}

