#pragma once
#include <utils/nlohmann/json.hpp>


namespace mqttMessageTemplates {


    using json = nlohmann::json;

    inline json registration()
    {
        /**
         * Device sends this payload
         * Recives simple Ack
         * {
         *   "deviceId" : <same as device id>
         *   "resp": "ack", //ack or nack
         *   "reason": "" //optional cloud can give something incase of nack
         * }
         */
        return {

            {"deviceId",""}, //widelapse_<mac_address>
            {"deviceType",""},
        };
    }

    inline json heartBeat()
    {

        return {
        ""
        };
    }


}