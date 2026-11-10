target_include_directories(controlApp PRIVATE
${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(controlApp PRIVATE
    utils.cpp
    diagnostic.cpp
)
