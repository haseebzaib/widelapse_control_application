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

    inline json remoteCommand()
    {
        /**
         * We recive remoteCommand json like following from cloud
         * remote commands can be of different type does not matter but json follows this pattern
         * when we send reply we send same cmdType 
         * in cmdData can be json of ack like following
         * cmdData = {"status":"success/failed", "reason":""} //currently just following
         * on failed we can give some reason but later for that
         * 
         */

        return {
            {"cmdType",""},
            {"cmdData",json::object()}
        };
    }


}