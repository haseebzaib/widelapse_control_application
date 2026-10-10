#include "cameraController.hpp"

cameraController::cameraController()
{
    context_ = gp_context_new();
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
            return false;
        }
    }

    int ret = gp_camera_new(&camera_);
    if (ret < GP_OK)
    {
        return false;
    }

    ret = gp_camera_init(camera_, context_);
    if (ret < GP_OK)
    {
        std::cerr << gp_result_as_string(ret) << '\n';

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
        return {};

    CameraText summary{};

    int ret = gp_camera_get_summary(
        camera_,
        &summary,
        context_
    );

    if (ret < GP_OK)
        return {};

    return summary.text;
}


bool cameraController::capture_image(const std::string& filename)
{
    if (!connected_)
        return false;

    CameraFilePath path{};

    int ret = gp_camera_capture(
        camera_,
        GP_CAPTURE_IMAGE,
        &path,
        context_
    );

    if (ret < GP_OK)
        return false;

    CameraFile* file = nullptr;

    ret = gp_file_new(&file);

    if (ret < GP_OK)
        return false;

    ret = gp_camera_file_get(
        camera_,
        path.folder,
        path.name,
        GP_FILE_TYPE_NORMAL,
        file,
        context_
    );

    if (ret >= GP_OK)
        ret = gp_file_save(file, filename.c_str());

    gp_file_free(file);

    return ret >= GP_OK;
}


bool cameraController::create_context()
{
    context_ = gp_context_new();
    if (!context_)
    {
        return false;
    }

    return true;
}