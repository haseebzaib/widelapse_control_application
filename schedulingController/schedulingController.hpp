#pragma once

#include "string"
#include "format"
#include "iostream"
#include <functional>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

class schedulingController
{

public:
    schedulingController(std::string controllerName);
    ~schedulingController();

    schedulingController(const schedulingController &) = delete;
    schedulingController &operator=(const schedulingController &) = delete;


    /*
    "*" -> this indicates that thing runs continously.
    validation_rules: startime and endtime together needs to be *, one cant be 
     */
    struct scheduleProfile
    {
        std::string name;
        std::vector<std::uint8_t> weekDays;
        std::chrono::minutes startTime;
        std::chrono::minutes endTime;
        std::chrono::minutes interval;
        std::function<void()> callBack;
    };

    void add_profile(scheduleProfile addProfile);


private:
    void scheduler_loop(std::stop_token stopToken);

    struct logLevel
    {
        static constexpr std::string_view info = "INFO";
        static constexpr std::string_view warn = "WARN";
        static constexpr std::string_view err = "ERR";
    };
    
    void logging(std::string_view loggingLevel, std::string msg);
    inline static const std::string logTag = "schedulingController";

    std::vector<scheduleProfile> addProfile_;
    std::string controllerName_;
    std::jthread worker_;

};