cmake_minimum_required(VERSION 3.16)

project(controlApp LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GPHOTO2 REQUIRED IMPORTED_TARGET libgphoto2)

add_executable(controlApp 
 main.cpp
)


target_include_directories(controlApp PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(camera)

target_link_libraries(controlApp
PRIVATE 
     PkgConfig::GPHOTO2
)