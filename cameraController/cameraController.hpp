#pragma once

#include "string"
#include "format"
#include "iostream"
#include "filesystem"
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

    bool capture_image(std::filesystem::path path);

private:
    bool create_context();

    static void error_cb(GPContext *context, const char *msg, void *data);
    static void status_cb(GPContext *context, const char *msg, void *data);
    static unsigned int progress_start_cb(GPContext *context, float target,
                                          const char *message,
                                          void *data);
    static void progress_update_cb(GPContext *context, unsigned int id,
                                   float current,
                                   void *data);
    static void progress_stop_cb(GPContext *context, unsigned int id,
                                 void *data);

    inline static const std::string logTag = "cameraController";
    Camera *camera_ = nullptr;
    GPContext *context_ = nullptr;
    bool connected_ = false;
};
