#include "mqttRemoteCommand.hpp"
#include "mqttController.hpp"
#include "mqttMessageTemplates.hpp"
#include <fstream>
#include <filesystem>
#include <limits>
#include <utility>
#include "utils/utils.hpp"
#include "schedulingController/schedulingController.hpp"
#include "main.hpp"
#include "cameraController/cameraController.hpp"
#include "sftpController/sftpController.hpp"

namespace cloudController
{

    namespace mqttRemoteCommand
    {
        static void wifi_setup(const nlohmann::json &data);
        static void scheduling_setup(const nlohmann::json &data);
        static void wifi_scan(const nlohmann::json &data);
        static void total_shots(const nlohmann::json &data);
        static void picture_uploads(const nlohmann::json &data);

        static const std::unordered_map<std::string, std::function<void(const nlohmann::json &)>> commands = {

            {"wifiSetup", wifi_setup},
            {"scheduling_setup", scheduling_setup},
            {"wifi_scan", wifi_scan},
            {"total_shots", total_shots},
            {"picture_uploads", picture_uploads},

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

        static schedulingController *cameraScheduler_ = nullptr;
        static schedulingController *transferScheduler_ = nullptr;
        static std::function<void()> capture_;
        static std::function<void()> transfer_;
        static std::string schedulingPath_;

        static int schedule_integer(const nlohmann::json &object, const char *key,
                                    int minimum, int maximum)
        {
            const auto &value = object.at(key);
            if (!value.is_number_integer() || value < minimum || value > maximum)
                throw std::invalid_argument(std::string("Invalid schedule field: ") + key);
            return value.get<int>();
        }

        static void apply_schedules(const nlohmann::json &data, bool save)
        {
            using namespace std::chrono;
            if (!cameraScheduler_ || !transferScheduler_ || !capture_ || !transfer_)
                throw std::runtime_error("Scheduling callbacks are not initialized");

            const auto &schedules = data.at("cmdData").at("schedule");
            if (!schedules.is_array())
                throw std::invalid_argument("schedule must be an array");

            std::vector<schedulingController::scheduleProfile> cameraProfiles;
            std::vector<schedulingController::scheduleProfile> transferProfiles;
            // Build and validate every profile before changing the file or live schedules.
            for (const auto &item : schedules)
            {
                const auto type = item.at("type").get<std::string>();
                if (type != "camera" && type != "transfer")
                    throw std::invalid_argument("Schedule type must be camera or transfer");

                const auto &days = item.at("weekDays");
                if (!days.is_array())
                    throw std::invalid_argument("weekDays must be an array");
                std::vector<std::uint8_t> weekDays;
                for (const auto &day : days)
                {
                    if (!day.is_number_integer() || day < -1 || day > 6)
                        throw std::invalid_argument("Weekday must be -1 or 0 through 6");
                    const int weekday = day.get<int>();
                    if (weekday == -1)
                    {
                        if (days.size() != 1)
                            throw std::invalid_argument("Use [-1] alone for every day");
                        // Empty internal weekday vector means every day.
                    }
                    else
                        weekDays.push_back(static_cast<std::uint8_t>(weekday));
                }

                const int start = schedule_integer(item, "startTime", 0, 1439);
                const int end = schedule_integer(item, "endTime", 1, 1440);
                const int interval = schedule_integer(item, "interval", 1, std::numeric_limits<int>::max());
                if (start >= end)
                    throw std::invalid_argument("startTime must be less than endTime");

                schedulingController::scheduleProfile profile{
                    type, weekDays, minutes(start), minutes(end), seconds(interval),
                    type == "camera" ? capture_ : transfer_};
                if (type == "camera")
                    cameraProfiles.push_back(std::move(profile));
                else
                    transferProfiles.push_back(std::move(profile));
            }

            if (save)
            {
                const std::filesystem::path path(schedulingPath_);
                if (!path.parent_path().empty())
                    std::filesystem::create_directories(path.parent_path());
                const auto temporaryPath = schedulingPath_ + ".tmp";
                if (!utils::save_json(temporaryPath, data))
                {
                    std::error_code error;
                    std::filesystem::remove(temporaryPath, error);
                    throw std::runtime_error("Unable to save scheduling.json");
                }
                // Keep the previous saved file intact until the replacement is written.
                std::filesystem::rename(temporaryPath, path);
            }

            cameraScheduler_->clear_profiles();
            transferScheduler_->clear_profiles();
            for (auto &profile : cameraProfiles)
                cameraScheduler_->add_profile(std::move(profile));
            for (auto &profile : transferProfiles)
                transferScheduler_->add_profile(std::move(profile));
            logging(logLevel::info, std::format("schedules applied: camera={}, transfer={}",
                                                cameraProfiles.size(), transferProfiles.size()));
        }

        void init_scheduling(schedulingController &cameraScheduler,
                             schedulingController &transferScheduler,
                             std::function<void()> capture,
                             std::function<void()> transfer,
                             std::string path)
        {
            cameraScheduler_ = &cameraScheduler;
            transferScheduler_ = &transferScheduler;
            capture_ = std::move(capture);
            transfer_ = std::move(transfer);
            schedulingPath_ = std::move(path);

            const auto saved = utils::read_json(schedulingPath_);
            if (saved.is_null())
            {
                logging(logLevel::warn, "No readable scheduling.json; waiting for scheduling_setup");
                return;
            }
            try
            {
                apply_schedules(saved, false);
            }
            catch (const std::exception &error)
            {
                logging(logLevel::err, std::string("Cannot load scheduling.json: ") + error.what());
            }
        }

        static void scheduling_setup(const nlohmann::json &data)
        {
            // Full replacement: times are minutes since midnight, intervals are seconds.
            // weekDays: [-1] or [] means every day; otherwise use Sunday=0 ... Saturday=6.
            logging(logLevel::info, "inside scheduling setup cb");
            nlohmann::json reply = mqttMessageTemplates::remoteCommand();
            reply["cmdType"] = "scheduling_setup";
            try
            {
                apply_schedules(data, true);
                reply["cmdData"] = {{"status", "success"}, {"reason", ""}};
            }
            catch (const std::exception &error)
            {
                logging(logLevel::err, error.what());
                reply["cmdData"] = {{"status", "failure"}, {"reason", error.what()}};
            }
            mqttController::remote_command_publish(reply.dump());
        }

        static void wifi_scan(const nlohmann::json &data)
        {

            logging(logLevel::info, "inside wifi scan cb");
            nlohmann::json reply = mqttMessageTemplates::remoteCommand();
            reply["cmdType"] = "wifi_scan";
            try
            {

                reply["cmdData"] = {
                    {"status", "success"},
                    {"data", utils::getWifiScan()},
                    {"reason", ""}};
            }
            catch (const std::exception &error)
            {
                logging(logLevel::err, error.what());
                reply["cmdData"] = {{"status", "failure"}, {"reason", error.what()}};
            }
            mqttController::remote_command_publish(reply.dump());
        }

        static void total_shots(const nlohmann::json &)
        {
            nlohmann::json reply = mqttMessageTemplates::remoteCommand();
            reply["cmdType"] = "total_shots";
            try
            {
                const auto shots = mainCamera.get_shots_info();
                reply["cmdData"] = {
                    {"status", "success"},
                    {"data", {{"totalShotsTaken", shots.totalShotsTaken},
                              {"totalShotsFailed", shots.totalShotsFailed}}},
                    {"reason", ""}};
            }
            catch (const std::exception &error)
            {
                logging(logLevel::err, error.what());
                reply["cmdData"] = {{"status", "failure"}, {"reason", error.what()}};
            }
            mqttController::remote_command_publish(reply.dump());
        }
        static void picture_uploads(const nlohmann::json &data)
        {

            logging(logLevel::info, "inside wifi scan cb");
            nlohmann::json reply = mqttMessageTemplates::remoteCommand();
            reply["cmdType"] = "picture_uploads";
            try
            {

                sftpController::imageDirectoryInfo uploadInfo = mainSftpServer.image_direcotry_info();

                nlohmann::json uploadData = {
                    {"totalImagesTaken", uploadInfo.totalImages},
                    {"totalImagesTransfered", uploadInfo.totalImagesTransfered},
                    {"totalImagesFailed", uploadInfo.totalImagesFailed}};

                reply["cmdData"] = {
                    {"status", "success"},
                    {"data", uploadData},
                    {"reason", ""}};
            }
            catch (const std::exception &error)
            {
                logging(logLevel::err, error.what());
                reply["cmdData"] = {{"status", "failure"}, {"reason", error.what()}};
            }
            mqttController::remote_command_publish(reply.dump());
        }

        std::unordered_map<std::string, std::function<void(const nlohmann::json &)>> get_commands_callbacks()
        {

            return commands;
        }

    }
}
