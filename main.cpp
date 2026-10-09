#include "iostream"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "format"
#include "gphoto2/gphoto2-camera.h"




static void camera_error_func(GPContext *context,const char *str, void *data)
{
    std::cout << std::format("****Context error***\n***{}***\n",str);
}

static void camera_status_func(GPContext *context,const char *str,void *data)
{
    std::cout << std::format("***Context status***\n***{}***\n",str);
}


GPContext * camera_create_context()
{
  GPContext *context;
  context = gp_context_new();
  gp_context_set_error_func(context,camera_error_func,nullptr);
  gp_context_set_status_func(context,camera_status_func,nullptr);

  return context;
}



int main()
{
 Camera *cam;
 int ret;
 char *owner;
 GPContext *context;
 CameraText text;


 context = camera_create_context();

 gp_camera_new(&cam);
 ret = gp_camera_init(cam,context);
 if(ret < GP_OK)
 {
      std::cout << std::format(
        "gp_camera_init failed: {} ({})\n",
        gp_result_as_string(ret),
        ret
    );
    gp_camera_free(cam);
    return 0;
 }

 ret = gp_camera_get_summary(cam,&text,context);
 if(ret < GP_OK)
 {
     std::cout << std::format("camera failed retrieving the summary \n");
     gp_camera_free(cam);
     return 0;
 }

 std::cout << std::format("Summary:{} \n",text.text);

	gp_camera_exit (cam, context);
	gp_camera_free (cam);
	gp_context_unref (context);
	return 0;



}


