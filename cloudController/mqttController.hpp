#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "mqtt/async_client.h"

namespace cloudController
{

    namespace mqttController
    {

        const std::string serverUri("mqtt://broker.hivemq.com:1883");
        const std::string serverPort("1883");
        const std::string subTopic("widelapse/1");
        const int qos = 1;
        const int n_retry_attempts = 5;

        

        /*Starts A thread*/
        void init();
        void stop();


    }

}