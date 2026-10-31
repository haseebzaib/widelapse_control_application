#include "schedulingController/schedulingController.hpp"

#include <atomic>
#include <condition_variable>
#include <ctime>
#include <iostream>
#include <stdexcept>

int main()
{
    using namespace std::chrono;
    std::mutex mutex;
    std::condition_variable changed;
    std::atomic<int> runs{0};
    std::atomic<int> excludedRuns{0};
    std::atomic<bool> slowStarted{false};
    std::atomic<bool> fastRan{false};
    std::atomic<bool> addedRan{false};

    {
        schedulingController slow("slow");
        schedulingController fast("fast");
        auto invalid = schedulingController::scheduleProfile{
            "invalid", {}, hours(0), hours(24), minutes(0), [] {}};
        const auto rejected = [&](auto profile)
        {
            try { fast.add_profile(profile); }
            catch (const std::invalid_argument &) { return true; }
            return false;
        };
        if (!rejected(invalid))
            throw std::runtime_error("zero interval accepted");
        invalid.interval = minutes(1);
        invalid.weekDays = {7};
        if (!rejected(invalid))
            throw std::runtime_error("invalid weekday accepted");
        invalid.weekDays = {};
        invalid.startTime = hours(24);
        if (!rejected(invalid))
            throw std::runtime_error("empty window accepted");
        invalid.startTime = hours(0);
        invalid.callBack = {};
        if (!rejected(invalid))
            throw std::runtime_error("empty callback accepted");

        const auto time = system_clock::to_time_t(system_clock::now());
        std::tm local{};
        localtime_r(&time, &local);
        fast.add_profile({"excluded weekday", {static_cast<std::uint8_t>((local.tm_wday + 1) % 7)},
                          hours(0), hours(24), minutes(1), [&] { ++excludedRuns; }});
        // This callback adds a profile while running, verifying callbacks hold no profile lock.
        slow.add_profile({"slow callback", {}, hours(0), hours(24), minutes(1), [&]
        {
            ++runs;
            slowStarted = true;
            slow.add_profile({"added callback", {}, hours(0), hours(24), minutes(1), [&]
            {
                addedRan = true;
                changed.notify_all();
            }});
            std::unique_lock lock(mutex);
            if (!changed.wait_for(lock, seconds(5), [&] { return fastRan.load(); }))
                throw std::runtime_error("independent scheduler was blocked");
        }});
        fast.add_profile({"fast callback", {}, hours(0), hours(24), minutes(1), [&]
        {
            if (slowStarted)
            {
                fastRan = true;
                changed.notify_all();
            }
            else
            {
                // First run can happen before the other scheduler starts.
                fast.add_profile({"fast retry", {}, hours(0), hours(24), minutes(1), [&]
                {
                    fastRan = true;
                    changed.notify_all();
                }});
            }
        }});
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, seconds(8), [&] { return addedRan.load() && fastRan.load(); }))
            throw std::runtime_error("callbacks did not complete");
    } // Workers join before callback resources are destroyed.
    if (runs != 1 || excludedRuns != 0)
        throw std::runtime_error("profile ran outside its weekday or before its interval");
    std::cout << "Scheduler checks passed\n";
}
