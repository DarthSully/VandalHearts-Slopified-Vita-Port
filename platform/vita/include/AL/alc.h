/* Minimal ALC subset for the PS Vita build (see AL/al.h). */
#ifndef VH_VITA_ALC_H
#define VH_VITA_ALC_H
#include "al.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ALCdevice ALCdevice;
typedef struct ALCcontext ALCcontext;
typedef char ALCboolean;
typedef char ALCchar;
typedef int ALCint;

ALCdevice *alcOpenDevice(const ALCchar *devicename);
ALCboolean alcCloseDevice(ALCdevice *device);
ALCcontext *alcCreateContext(ALCdevice *device, const ALCint *attrlist);
ALCboolean alcMakeContextCurrent(ALCcontext *context);
void alcDestroyContext(ALCcontext *context);
ALCcontext *alcGetCurrentContext(void);

#ifdef __cplusplus
}
#endif
#endif
