#include "cloudController/mqttRemoteCommand.hpp"
#include "utils/utils.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <unistd.h>

using nlohmann::json;
static json reply;

// Capture replies without connecting to an MQTT broker.
namespace cloudController::mqttController
{
    void remote_command_publish(std::string message) { reply = json::parse(message); }
}

static void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main()
{
    using namespace std::chrono;
    const auto directory = std::filesystem::temp_directory_path() /
        ("widelapse-schedule-test-" + std::to_string(getpid()));
    std::filesystem::remove_all(directory);
    const auto path = (directory / "configs" / "scheduling.json").string();
    std::atomic<int> captures{0}, transfers{0};
    {
        schedulingController camera("test camera"), transfer("test transfer");
        const auto initialize = [&]
        {
            cloudController::mqttRemoteCommand::init_scheduling(
                camera, transfer, [&] { ++captures; }, [&] { ++transfers; }, path);
        };
        const auto command = cloudController::mqttRemoteCommand::get_commands_callbacks().at("scheduling_setup");
        initialize(); // Missing file is allowed.
        require(!std::filesystem::exists(path), "startup wrote an unsolicited config");

        json schedules = json::array();
        for (int i = 0; i < 5; ++i)
            schedules.push_back({{"type", i < 3 ? "camera" : "transfer"},
                                 {"weekDays", {-1}}, {"startTime", 0},
                                 {"endTime", 1440}, {"interval", 60}});
        const json data = {{"cmdType", "scheduling_setup"},
                           {"cmdData", {{"schedule", schedules}}}};
        command(data);
        require(reply["cmdData"]["status"] == "success", "valid command failed");
        require(utils::read_json(path) == data, "full command was not saved");
        std::this_thread::sleep_for(seconds(2));
        require(captures == 3 && transfers == 2, "profiles dispatched to wrong callbacks");

        // Invalid entries must not overwrite the file or replace valid profiles.
        for (const auto &field : {"type", "weekDays", "startTime", "endTime", "interval"})
        {
            auto invalid = data;
            auto &entry = invalid["cmdData"]["schedule"][4];
            const std::string key(field);
            if (key == "type") entry[key] = "unknown";
            else if (key == "weekDays") entry[key] = {-1, 2};
            else if (key == "startTime") entry[key] = 1440;
            else if (key == "endTime") entry[key] = 0;
            else entry[key] = 0;
            command(invalid);
            require(reply["cmdData"]["status"] == "failure", "invalid profile accepted");
            require(utils::read_json(path) == data, "invalid command overwrote saved schedule");
        }

        // Reinitializing from the saved file follows the startup reload path.
        initialize();
        std::this_thread::sleep_for(seconds(2));
        require(captures == 6 && transfers == 4, "startup reload duplicated or lost profiles");

        const json empty = {{"cmdType", "scheduling_setup"},
                            {"cmdData", {{"schedule", json::array()}}}};
        command(empty);
        require(reply["cmdData"]["status"] == "success", "clearing schedules failed");
        require(utils::read_json(path) == empty, "empty replacement was not saved");
        initialize();
        std::this_thread::sleep_for(seconds(2));
        require(captures == 6 && transfers == 4, "cleared profiles ran again");

        // A failed save must return failure and preserve the existing file.
        const auto blocked = directory / "blocked";
        require(utils::save_json(blocked.string(), json::object()), "fixture save failed");
        cloudController::mqttRemoteCommand::init_scheduling(
            camera, transfer, [&] { ++captures; }, [&] { ++transfers; },
            (blocked / "scheduling.json").string());
        command(data);
        require(reply["cmdData"]["status"] == "failure", "failed save reported success");
        require(utils::read_json(path) == empty, "failed save damaged previous config");
    }
    std::filesystem::remove_all(directory);
}
