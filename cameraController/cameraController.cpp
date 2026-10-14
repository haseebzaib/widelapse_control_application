#include "cameraController.hpp"
#include <fcntl.h>
#include <unistd.h>
cameraController::cameraController()
{
    context_ = gp_context_new();
    if (!context_)
    {
        return;
    }

    gp_context_set_error_func(
        context_,
        error_cb,
        this);

    gp_context_set_status_func(
        context_,
        status_cb,
        this);

    gp_context_set_progress_funcs(
        context_,
        progress_start_cb,
        progress_update_cb,
        progress_stop_cb,
        this);
}

cameraController::~cameraController()
{
    disconnect();
    if (context_)
    {
        gp_context_unref(context_);
    }
}

bool cameraController::connect()
{
    if (connected_)
    {
        return true;
    }

    if (!context_)
    {
        if (!create_context())
        {
            logging(cameraController::logLevel::err, std::format("creating context failed"));
            return false;
        }
    }

    disconnect();

    int ret = gp_camera_new(&camera_);
    if (ret < GP_OK)
    {
        logging(cameraController::logLevel::err, std::format("camera new failed reason:{}", gp_result_as_string(ret)));
        return false;
    }

    ret = gp_camera_init(camera_, context_);
    if (ret < GP_OK)
    {

        logging(cameraController::logLevel::err, std::format("camera init failed reason:{}", gp_result_as_string(ret)));
        gp_camera_free(camera_);
        camera_ = nullptr;

        return false;
    }

    connected_ = true;
    return true;
}

void cameraController::disconnect()
{
    if (!camera_)
        return;

    if (connected_)
        gp_camera_exit(camera_, context_);

    gp_camera_free(camera_);

    camera_ = nullptr;
    connected_ = false;
}

std::string cameraController::get_summary()
{
    if (!connected_)
    {
        logging(cameraController::logLevel::warn, std::format("cant summary not connected"));
        return {};
    }

    CameraText summary{};

    int ret = gp_camera_get_summary(
        camera_,
        &summary,
        context_);

    if (ret < GP_OK)
    {
        logging(cameraController::logLevel::err, std::format("summary get failed reason:{}", gp_result_as_string(ret)));
        return {};
    }

    return summary.text;
}

bool cameraController::capture_image(std::filesystem::path path)
{
    if (!connected_)
    {
        if (!connect())
        {
            logging(cameraController::logLevel::warn, std::format("cant capture not connected"));
            return false;
        }
    }

    if (!std::filesystem::exists(path.parent_path()))
    {
        logging(cameraController::logLevel::warn, std::format("Directory does not exists, creating one"));
        std::filesystem::create_directory(path.parent_path());
    }

    CameraFilePath path_{};

    int ret = gp_camera_capture(
        camera_,
        GP_CAPTURE_IMAGE,
        &path_,
        context_);

    if (ret < GP_OK)
    {
        logging(cameraController::logLevel::err, std::format("capture failed reason:{}", gp_result_as_string(ret)));
        disconnect();
        return false;
    }

    CameraFile *file = nullptr;

    int fd = open(path.string().c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);

    ret = gp_file_new_from_fd(&file, fd);

    if (ret < GP_OK)
    {

        logging(cameraController::logLevel::err, std::format("capture new file failed reason:{}", gp_result_as_string(ret)));
        disconnect();
        return false;
    }

    ret = gp_camera_file_get(
        camera_,
        path_.folder,
        path_.name,
        GP_FILE_TYPE_NORMAL,
        file,
        context_);

    if (ret >= GP_OK)
    {
        ret = gp_camera_file_delete(camera_, path_.folder, path_.name, context_);
        if (ret < GP_OK)
        {
            logging(cameraController::logLevel::err, std::format("capture file deletion failed reason:{}", gp_result_as_string(ret)));
        }
    }
    else
    {
        logging(cameraController::logLevel::err, std::format("capture file failed reason:{}", gp_result_as_string(ret)));
        disconnect();
    }

    gp_file_free(file);

    return ret >= GP_OK;
}

void cameraController::logging(std::string_view loggingLevel, std::string msg)
{

    std::cout << std::format("[{}][{}] {}\n", loggingLevel, logTag, msg);
}

bool cameraController::create_context()
{
    context_ = gp_context_new();
    if (!context_)
    {
        logging(cameraController::logLevel::err, "Could not create context");
        return false;
    }

    return true;
}

void cameraController::error_cb(
    GPContext *context,
    const char *message,
    void *data)
{

    cameraController *self = static_cast<cameraController *>(data);
    if (!self)
    {
        return;
    }

    self->logging(cameraController::logLevel::err, message);
}

void cameraController::status_cb(
    GPContext *context,
    const char *message,
    void *data)
{
    cameraController *self = static_cast<cameraController *>(data);
    if (!self)
    {
        return;
    }

    self->logging(cameraController::logLevel::info, message);
}

unsigned int cameraController::progress_start_cb(
    GPContext *context,
    float target,
    const char *message,
    void *data)
{
    cameraController *self = static_cast<cameraController *>(data);
    if (!self)
    {
        return 0;
    }

    self->logging(cameraController::logLevel::info, message);
    return 0;
}

void cameraController::progress_update_cb(
    GPContext *context,
    unsigned int id,
    float current,
    void *data)
{
    cameraController *self = static_cast<cameraController *>(data);
    if (!self)
    {
        return;
    }

    self->logging(cameraController::logLevel::info, std::format("progress id{},current{}", id, current));
}

void cameraController::progress_stop_cb(
    GPContext *context,
    unsigned int id,
    void *data)
{
    cameraController *self = static_cast<cameraController *>(data);
    if (!self)
    {
        return;
    }

    self->logging(cameraController::logLevel::info, std::format("progress id{}", id));
}