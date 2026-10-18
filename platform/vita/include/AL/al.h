/* Minimal OpenAL 1.1 API subset for the PS Vita build, implemented over SDL2 audio by
 * platform/vita/src/vita_al.c. Only what platform/pc/src (libsnd.c, pc_spu.c, pc_xa.c) uses:
 * buffers, queued streaming sources, per-source and listener gain. */
#ifndef VH_VITA_AL_H
#define VH_VITA_AL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef char ALboolean;
typedef char ALchar;
typedef int ALint;
typedef unsigned int ALuint;
typedef int ALsizei;
typedef int ALenum;
typedef float ALfloat;
typedef void ALvoid;

#define AL_NONE 0
#define AL_FALSE 0
#define AL_TRUE 1
#define AL_NO_ERROR 0

#define AL_GAIN 0x100A
#define AL_SOURCE_STATE 0x1010
#define AL_INITIAL 0x1011
#define AL_PLAYING 0x1012
#define AL_PAUSED 0x1013
#define AL_STOPPED 0x1014
#define AL_BUFFERS_QUEUED 0x1015
#define AL_BUFFERS_PROCESSED 0x1016

#define AL_FORMAT_MONO8 0x1100
#define AL_FORMAT_MONO16 0x1101
#define AL_FORMAT_STEREO8 0x1102
#define AL_FORMAT_STEREO16 0x1103

void alGenBuffers(ALsizei n, ALuint *buffers);
void alDeleteBuffers(ALsizei n, const ALuint *buffers);
void alBufferData(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei freq);
void alBufferiv(ALuint buffer, ALenum param, const ALint *values);

void alGenSources(ALsizei n, ALuint *sources);
void alDeleteSources(ALsizei n, const ALuint *sources);
void alSourcef(ALuint source, ALenum param, ALfloat value);
void alGetSourcei(ALuint source, ALenum param, ALint *value);
void alSourceQueueBuffers(ALuint source, ALsizei n, const ALuint *buffers);
void alSourceUnqueueBuffers(ALuint source, ALsizei n, ALuint *buffers);
void alSourcePlay(ALuint source);
void alSourceStop(ALuint source);
void alSourcePause(ALuint source);

void alListenerf(ALenum param, ALfloat value);
ALenum alGetError(void);

#ifdef __cplusplus
}
#endif
#endif
