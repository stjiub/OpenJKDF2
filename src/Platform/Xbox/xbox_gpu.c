#include "xbox_gpu.h"

#include <hal/video.h>
#include <hal/debug.h>
#include <pbgl.h>

static int xbox_gpu_bInitted = 0;

int xbox_gpu_init(void)
{
    int err;

    if (xbox_gpu_bInitted)
        return 1;

    if (!XVideoSetMode(640, 480, 32, REFRESH_DEFAULT)) {
        debugPrint("xbox_gpu_init: video mode unavailable\n");
        return 0;
    }

    err = pbgl_init(1);
    if (err < 0) {
        debugPrint("xbox_gpu_init: pbgl_init() failed: %d\n", err);
        return 0;
    }
    pbgl_set_swap_interval(1);

    xbox_gpu_bInitted = 1;
    return 1;
}

void xbox_gpu_shutdown(void)
{
    if (!xbox_gpu_bInitted)
        return;

    pbgl_shutdown();
    xbox_gpu_bInitted = 0;
}
