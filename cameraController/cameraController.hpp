#pragma once

#include "string"
#include "format"
#include "iostream"
#include "gphoto2/gphoto2-camera.h"
#include "gphoto2/gphoto2-widget.h"
#include "gphoto2/gphoto2-context.h"
#include "gphoto2/gphoto2-setting.h"
#include "gphoto2/gphoto2-abilities-list.h"
#include "gphoto2/gphoto2-file.h"

class cameraController
{

public:
    cameraController();
    ~cameraController();

    cameraController(const cameraController &) = delete;
    cameraController &operator=(const cameraController &) = delete;

    bool connect();
    void disconnect();

    std::string get_summary();

    bool capture_image(const std::string &filename);

private:
    bool create_context();

    Camera *camera_ = nullptr;
    GPContext *context_ = nullptr;
    bool connected_ = false;
};
