#include "schedulingController.hpp"

schedulingController::schedulingController(std::string controllerName)
{
    controllerName_ = controllerName;
    worker_ = std::jthread([this](std::stop_token stopToken)
                           { scheduler_loop(stopToken); });
}

schedulingController::~schedulingController()
{
}

void schedulingController::scheduler_loop(std::stop_token stopToken)
{

    while (!stopToken.stop_requested())
    {

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    logging(logLevel::info, "schedulingController loop has stopped");
}

void schedulingController::logging(std::string_view loggingLevel, std::string msg)
{
    std::cout << std::format("[{}][{}][{}] {}\n", loggingLevel, logTag, controllerName_,msg);
}