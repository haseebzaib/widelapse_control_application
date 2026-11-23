cmake_minimum_required(VERSION 3.16)

project(controlApp LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(PahoMqttCpp REQUIRED)

pkg_check_modules(GPHOTO2 REQUIRED IMPORTED_TARGET libgphoto2)
pkg_check_modules(LIBSSH2 REQUIRED IMPORTED_TARGET libssh2)



add_executable(controlApp 
 main.cpp
 mainCamera.cpp
 mainSftpServer.cpp
)


target_include_directories(controlApp PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(cameraController)
add_subdirectory(sftpController)
add_subdirectory(schedulingController)
add_subdirectory(cloudController)
add_subdirectory(utils)

target_link_libraries(controlApp
PRIVATE 
     PkgConfig::GPHOTO2
     PkgConfig::LIBSSH2
     Threads::Threads
     PahoMqttCpp::paho-mqttpp3
)
include(CTest)
if(BUILD_TESTING)
    add_executable(schedulingController_test
        tests/schedulingController_test.cpp
        schedulingController/schedulingController.cpp
    )
    target_include_directories(schedulingController_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(schedulingController_test PRIVATE Threads::Threads)
    add_test(NAME schedulingController COMMAND schedulingController_test)
    add_executable(scheduling_setup_test
        tests/scheduling_setup_test.cpp
        cameraController/cameraController.cpp
        cloudController/mqttRemoteCommand.cpp
        mainCamera.cpp
        mainSftpServer.cpp
        schedulingController/schedulingController.cpp
        sftpController/sftpController.cpp
        utils/utils.cpp
    )
    target_include_directories(scheduling_setup_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(scheduling_setup_test PRIVATE
        PkgConfig::GPHOTO2
        PkgConfig::LIBSSH2
        Threads::Threads
        PahoMqttCpp::paho-mqttpp3
    )
    add_test(NAME scheduling_setup COMMAND scheduling_setup_test)
endif()
