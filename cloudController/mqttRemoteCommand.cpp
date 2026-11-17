#include "mqttRemoteCommand.hpp"
#include "mqttController.hpp"
#include "mqttMessageTemplates.hpp"

namespace cloudController
{

    namespace mqttRemoteCommand
    {
        static void wifi_setup(const nlohmann::json &data);

        static const std::unordered_map<std::string, std::function<void(const nlohmann::json &)>> commands = {

            {"wifiSetup", wifi_setup}};

        static const std::string logTag = "mqttRemoteCommand";
        struct logLevel
        {
            static constexpr std::string_view info = "INFO";
            static constexpr std::string_view warn = "WARN";
            static constexpr std::string_view err = "ERR";
        };

        static void logging(std::string_view loggingLevel, std::string msg)
        {
            std::cout << std::format("[{}][{}] {}\n", loggingLevel, logTag, msg);
        }

        static void wifi_setup(const nlohmann::json &data)
        {

            logging(logLevel::info, "inside wifi setup cb");



            nlohmann::json reply = mqttMessageTemplates::remoteCommand();

            reply["cmdType"] = data["cmdType"];
            reply["cmdData"] = {{"data","none"},{"justtesting","testing"},"lol"};

            std::string payloadSend = reply.dump(); 

            mqttController::remote_command_publish(payloadSend);
        }

        std::unordered_map<std::string, std::function<void(const nlohmann::json &)>> get_commands_callbacks()
        {

            return commands;
        }

    }
}