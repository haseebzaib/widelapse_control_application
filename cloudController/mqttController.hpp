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
        const std::string pubTopicRegistration("widelapse/registration"); //registration topic
        const std::string subTopicRegistration("widelapse/registration/1");
        const int qos = 1;
        const int n_retry_attempts = 5;
        const int publishTimeout = 10000;
        const int registrationInterval = 10; //in seconds
        const int regularUpdateInterval = 60; //inseconds

        

        /*Starts A thread*/
        void init();
        void stop();


    }

}