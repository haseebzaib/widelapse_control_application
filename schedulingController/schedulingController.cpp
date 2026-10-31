#include "schedulingController.hpp"

#include <algorithm>
#include <ctime>
#include <format>
#include <iostream>
#include <stdexcept>
#include <utility>

schedulingController::schedulingController(std::string controllerName)
    : controllerName_(std::move(controllerName)),
      worker_([this](std::stop_token stopToken) { scheduler_loop(stopToken); })
{
}

schedulingController::~schedulingController()
{
    worker_.request_stop();
    worker_.join();
}

void schedulingController::add_profile(scheduleProfile profile)
{
    using namespace std::chrono;
    if (!profile.callBack || profile.interval <= minutes::zero() ||
        profile.startTime < minutes::zero() || profile.endTime > hours(24) ||
        profile.startTime >= profile.endTime ||
        std::any_of(profile.weekDays.begin(), profile.weekDays.end(),
                    [](auto day) { return day > 6; }))
    {
        throw std::invalid_argument("Invalid schedule profile: " + profile.name);
    }

    std::lock_guard lock(profilesMutex_);
    profiles_.push_back({std::move(profile), {}});
    const auto &addedProfile = profiles_.back().profile;
    std::string weekDays;
    for (const auto day : addedProfile.weekDays)
    {
        if (!weekDays.empty())
            weekDays += ", ";
        weekDays += std::to_string(day);
    }
    if (weekDays.empty())
        weekDays = "every day";

    logging(logLevel::info,
            std::format("profile added: name={}, weekdays=[{}], start={:02}:{:02}, "
                        "end={:02}:{:02}, interval={} minutes, callback=set",
                        addedProfile.name, weekDays,
                        addedProfile.startTime.count() / 60, addedProfile.startTime.count() % 60,
                        addedProfile.endTime.count() / 60, addedProfile.endTime.count() % 60,
                        addedProfile.interval.count()));
}

void schedulingController::scheduler_loop(std::stop_token stopToken)
{
    using namespace std::chrono;
    while (!stopToken.stop_requested())
    {
        const auto now = system_clock::now();
        const auto time = system_clock::to_time_t(now);
        std::tm localTime{};
        localtime_r(&time, &localTime);
        const auto timeOfDay = hours(localTime.tm_hour) + minutes(localTime.tm_min);
        std::vector<std::function<void()>> callbacks;
        {
            std::lock_guard lock(profilesMutex_);
            for (auto &entry : profiles_)
            {
                const auto &profile = entry.profile;
                const bool activeDay = profile.weekDays.empty() ||
                    std::find(profile.weekDays.begin(), profile.weekDays.end(),
                              localTime.tm_wday) != profile.weekDays.end();
                if (!activeDay || timeOfDay < profile.startTime || timeOfDay >= profile.endTime)
                {
                    entry.nextRun = {};
                    continue;
                }
                if (now >= entry.nextRun)
                {
                    // Run immediately on entering a window, then at the interval.
                    // Missed runs are skipped rather than replayed in a burst.
                    entry.nextRun = now + profile.interval;
                    callbacks.push_back(profile.callBack);
                }
            }
        }

        // Callbacks run outside the lock so profiles can be added while working.
        for (const auto &callback : callbacks)
        {
            if (stopToken.stop_requested())
                break;
            try
            {
                callback();
            }
            catch (const std::exception &error)
            {
                logging(logLevel::err, error.what());
            }
            catch (...)
            {
                logging(logLevel::err, "callback failed");
            }
        }
        std::this_thread::sleep_for(seconds(1));
    }
    logging(logLevel::info, "schedulingController loop has stopped");
}

void schedulingController::logging(std::string_view loggingLevel, std::string msg)
{
    std::cout << std::format("[{}][{}][{}] {}\n", loggingLevel, logTag, controllerName_, msg);
}
