// hal/video.h pulls in xboxkrnl.h, whose Win32 typedefs clash with the
// engine's types.h, so video mode setup lives behind this plain-C boundary.
#ifndef _XBOX_GPU_H
#define _XBOX_GPU_H

#ifdef __cplusplus
extern "C" {
#endif

// Sets 640x480x32 and initializes pbgl. Idempotent.
int xbox_gpu_init(void);
void xbox_gpu_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // _XBOX_GPU_H
