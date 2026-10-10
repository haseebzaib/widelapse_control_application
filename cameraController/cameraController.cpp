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
            std::cout << std::format("[ERROR][{}] creating context failed\n", logTag);
            return false;
        }
    }

    int ret = gp_camera_new(&camera_);
    if (ret < GP_OK)
    {
        std::cout << std::format("[ERROR][{}] camera new failed\n", logTag, logTag, gp_result_as_string(ret));
        return false;
    }

    ret = gp_camera_init(camera_, context_);
    if (ret < GP_OK)
    {
        std::cout << std::format("[ERROR][{}] camera init failed reason:{}\n", logTag, gp_result_as_string(ret));
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
        std::cout << std::format("[ERROR][{}] cant summary not connected \n", logTag);
        return {};
    }

    CameraText summary{};

    int ret = gp_camera_get_summary(
        camera_,
        &summary,
        context_);

    if (ret < GP_OK)
    {
        std::cout << std::format("[ERROR][{}] summary get failed reason:{}\n", logTag, logTag, gp_result_as_string(ret));
        return {};
    }

    return summary.text;
}

bool cameraController::capture_image(std::filesystem::path path)
{
    if (!connected_)
    {
        std::cout << std::format("[ERROR][{}] cant capture not connected\n", logTag);
        return false;
    }

    if (!std::filesystem::exists(path.parent_path()))
    {
        std::cout << std::format("[WARN][{}] Directory does not exists, creating one\n", logTag);
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
        std::cout << std::format("[ERROR][{}] capture failed reason:{}\n", logTag, gp_result_as_string(ret));
        return false;
    }

    CameraFile *file = nullptr;

    int fd = open( path.string().c_str(),O_CREAT | O_WRONLY | O_TRUNC, 0644);

    ret = gp_file_new_from_fd(&file, fd);

    if (ret < GP_OK)
        {
            std::cout << std::format("[ERROR][{}] capture new file failed reason:{}\n",logTag,gp_result_as_string(ret));
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
                std::cout << std::format("[ERROR][{}] capture file deletion failed reason:{}\n", logTag, gp_result_as_string(ret));
            }
    }
    else
    {
         std::cout << std::format("[ERROR][{}] capture file failed reason:{}\n", logTag, gp_result_as_string(ret));
    }

    gp_file_free(file);

    return ret >= GP_OK;
}

bool cameraController::create_context()
{
    context_ = gp_context_new();
    if (!context_)
    {
        std::cout << std::format("[ERROR][{}] Could not create context\n", logTag);
        return false;
    }

    return true;
}

void cameraController::error_cb(
    GPContext *context,
    const char *message,
    void *data)
{
    std::cout << std::format("[ERROR][{}]cb {}\n", logTag, message);
}

void cameraController::status_cb(
    GPContext *context,
    const char *message,
    void *data)
{
    std::cout << std::format("[INFO][{}]cb {}\n", logTag, message);
}

unsigned int cameraController::progress_start_cb(
    GPContext *context,
    float target,
    const char *message,
    void *data)
{
    std::cout << std::format("[INFO][{}]cb progress target{},{}\n", logTag, target, message);

    return 0;
}

void cameraController::progress_update_cb(
    GPContext *context,
    unsigned int id,
    float current,
    void *data)
{
    std::cout << std::format("[INFO][{}]cb progress id{},current{}\n", logTag, id, current);
}

void cameraController::progress_stop_cb(
    GPContext *context,
    unsigned int id,
    void *data)
{
    std::cout << std::format("[INFO][{}]cb progress id{}\n", logTag, id);
}