#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

class schedulingController
{
public:
    explicit schedulingController(std::string controllerName);
    ~schedulingController();

    schedulingController(const schedulingController &) = delete;
    schedulingController &operator=(const schedulingController &) = delete;

    struct scheduleProfile
    {
        std::string name;
        // Local time: Sunday = 0, Monday = 1, ... Saturday = 6. Empty = every day.
        std::vector<std::uint8_t> weekDays;
        // Start inclusive, end exclusive. 00:00 to 24:00 means all day.
        // Overnight windows are not supported; use two profiles instead.
        std::chrono::minutes startTime;
        std::chrono::minutes endTime;
        std::chrono::minutes interval;
        std::function<void()> callBack;
    };

    // Throws std::invalid_argument for an invalid profile.
    void add_profile(scheduleProfile profile);

private:
    struct scheduledProfile
    {
        scheduleProfile profile;
        std::chrono::system_clock::time_point nextRun{};
    };

    void scheduler_loop(std::stop_token stopToken);

    struct logLevel
    {
        static constexpr std::string_view info = "INFO";
        static constexpr std::string_view warn = "WARN";
        static constexpr std::string_view err = "ERR";
    };

    void logging(std::string_view loggingLevel, std::string msg);
    inline static const std::string logTag = "schedulingController";

    std::vector<scheduledProfile> profiles_;
    std::string controllerName_;
    std::mutex profilesMutex_;
    std::jthread worker_;
};
