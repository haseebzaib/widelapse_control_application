#include "mqttRemoteCommand.hpp"
#include "mqttController.hpp"
#include "mqttMessageTemplates.hpp"
#include <fstream>
#include <filesystem>
#include "utils/utils.hpp"
#include "schedulingController/schedulingController.hpp"

namespace cloudController
{

    namespace mqttRemoteCommand
    {
        static void wifi_setup(const nlohmann::json &data);
        static void scheduling_setup(const nlohmann::json &data);

        static const std::unordered_map<std::string, std::function<void(const nlohmann::json &)>> commands = {

            {"wifiSetup", wifi_setup},
            {"scheduling_setup", scheduling_setup}

        };

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

            /**
             * cmdData: {
             * "ssid" : "",
             * "password": ""
             *
             * }
             */

            logging(logLevel::info, "inside wifi setup cb");

            const std::string path = "/opt/widelapse/configs/network.json";
            const std::string tempPath = path + ".tmp";

            nlohmann::json reply = mqttMessageTemplates::remoteCommand();

            reply["cmdType"] = data["cmdType"];

            try
            {
                std::string ssid = data.at("cmdData").at("ssid").get<std::string>();
                std::string password = data.at("cmdData").at("password").get<std::string>();

                // Read existing configuration
                std::ifstream file(path);

                if (!file.is_open())
                    throw std::runtime_error("Unable to open network.json");

                nlohmann::json config = nlohmann::json::parse(file);
                file.close();

                // Update Wi-Fi configuration
                config["wifi"]["networks"][0]["ssid"] = ssid;
                config["wifi"]["networks"][0]["password"] = password;

                // Write updated configuration
                std::ofstream outFile(tempPath);
                outFile.exceptions(std::ios::failbit | std::ios::badbit);

                std::filesystem::permissions(
                    tempPath,
                    std::filesystem::perms::owner_read |
                        std::filesystem::perms::owner_write);

                outFile << config.dump(4);
                outFile.close();

                std::filesystem::rename(tempPath, path);

                reply["cmdData"] = {
                    {"status", "success"},
                    {"reason", ""}};

                utils::reload_network();

                logging(logLevel::info, "WiFi configuration updated");
            }
            catch (const std::exception &e)
            {
                reply["cmdData"] = {
                    {"status", "failure"},
                    {"reason", "Unable to update WiFi configuration"}};

                logging(logLevel::err, "WiFi configuration update failed");
            }

            mqttController::remote_command_publish(reply.dump());
        }

        static void scheduling_setup(const nlohmann::json &data)
        {
            /**

            * cmdData contains the complete replacement schedule.
            * Validate the entire list before replacing the current schedule.
            * An empty schedule array clears all profiles.
            *
            * schedule: array of objects containing:
            *   type:      "camera" or "transfer"
            *   weekDays:  array of integers, Sunday=0 through Saturday=6
            *              -1 means every day
            *   startTime: integer minutes since midnight, 0..1439
            *   endTime:   integer minutes since midnight, 1..1440
            *   interval:  positive integer seconds
            *
            * startTime is inclusive; endTime is exclusive.
            * startTime must be less than endTime.
            * All weekday/time windows use the device's local timezone.
            * Overnight windows require two profiles.


             * cmdData {
             * "schedule" : [
             *   {
             *     "type" : "camera/transfer", //currently there are two types camera profile or sftp transfer whatever want to set according to that profile camera or sftp transfer happens
             *     "weekDay" : <int>,//  -1 = everyday Local time: Sunday = 0, Monday = 1, ... Saturday = 6. Empty = every day.
             *      // Start inclusive, end exclusive. 00:00 to 24:00 means all day.
                    // Overnight windows are not supported; use two profiles instead.
             *     "startTime" : <int>,
                   "endTime" : <int>,
                   "interval" : <int>
             *
             *   },
             *   {
             *      // and other time if want for example in hour 22-23 need to take pic with interval 1
             *   },
             * ]

             * }
             */

            logging(logLevel::info, "inside scheduling setup cb");
            const std::string path = "/opt/widelapse/configs/scheduling.json";
            nlohmann::json reply = mqttMessageTemplates::remoteCommand();
            reply["cmdType"] = data["cmdType"];
            reply["cmdData"] = {
                {"status", "success"},
                {"reason", ""}};

            int scheduleJsonSize = data.at("cmdData").at("schedule").size();
            logging(logLevel::info, std::format("number of schedules {}", scheduleJsonSize));

            for(int i = 0; i<5; i++)
            {
              
            }


            mqttController::remote_command_publish(reply.dump());
        }

        std::unordered_map<std::string, std::function<void(const nlohmann::json &)>> get_commands_callbacks()
        {

            return commands;
        }

    }
}