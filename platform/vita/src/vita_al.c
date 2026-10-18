/* OpenAL subset over SDL2 audio for the PS Vita build.
 *
 * The PC port uses OpenAL only as a sink. Synthesis (pc_spu.c, a software SPU), CD-XA decoding
 * (pc_xa.c) and reverb all run in software; each feeds one streaming source with small queued
 * buffers. libsnd.c also uploads every VAG sample as a buffer, but those buffers are never
 * attached to a source. So this file implements buffers, queued sources, gain and play state,
 * and mixes the playing sources in SDL's audio callback at 48 kHz (the Vita's native rate).
 *
 * Queue semantics follow OpenAL Soft, which pc_xa.c and pc_spu.c are written against:
 *  - A playing source whose queue runs dry goes AL_STOPPED with every buffer processed.
 *  - alSourceStop marks every queued buffer processed.
 *  - alSourcePlay on a stopped/initial source rewinds to the start of its queue.
 *  - On a stopped source AL_BUFFERS_PROCESSED reports every queued buffer.
 *  - Only processed buffers (from the queue front) can be unqueued. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "AL/al.h"
#include "AL/alc.h"

#define VAL_OUT_RATE 48000
#define VAL_OUT_FRAMES 1024
#define VAL_MAX_BUFFERS 4096
#define VAL_MAX_SOURCES 8
#define VAL_MAX_QUEUE 64

typedef struct {
    int used;
    short *pcm;       /* interleaved when channels == 2 */
    int frames;
    int channels;
    int rate;
} ValBuffer;

typedef struct {
    int used;
    ALenum state;
    float gain;
    ALuint queue[VAL_MAX_QUEUE];
    int queued;       /* buffers in the queue */
    int current;      /* index of the buffer being played; == processed count */
    unsigned long long pos; /* 32.32 fixed-point frame position in the current buffer */
} ValSource;

struct ALCdevice { int dummy; };
struct ALCcontext { int dummy; };

static ValBuffer s_buffers[VAL_MAX_BUFFERS];
static ValSource s_sources[VAL_MAX_SOURCES];
static float s_listenerGain = 1.0f;
static SDL_AudioDeviceID s_dev = 0;
static ALCdevice s_device;
static ALCcontext s_context;
static ALCcontext *s_current = NULL;

static void ValLock(void) {
    if (s_dev) SDL_LockAudioDevice(s_dev);
}
static void ValUnlock(void) {
    if (s_dev) SDL_UnlockAudioDevice(s_dev);
}

static ValBuffer *ValBuf(ALuint id) {
    if (id == 0 || id > VAL_MAX_BUFFERS || !s_buffers[id - 1].used) return NULL;
    return &s_buffers[id - 1];
}
static ValSource *ValSrc(ALuint id) {
    if (id == 0 || id > VAL_MAX_SOURCES || !s_sources[id - 1].used) return NULL;
    return &s_sources[id - 1];
}

/* ---- mixer (SDL audio thread; the API side holds the device lock) ---- */

static inline int ValClamp16(int v) {
    return v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
}

static void ValMixSource(ValSource *s, int *acc, int frames) {
    const float g = s->gain * s_listenerGain;
    const int gq = (int)(g * 256.0f); /* 8.8 fixed-point gain */
    int f = 0;
    while (f < frames && s->current < s->queued) {
        ValBuffer *b = ValBuf(s->queue[s->current]);
        if (!b || b->frames <= 0 || !b->pcm) {
            s->current++;
            s->pos = 0;
            continue;
        }
        const unsigned long long step = ((unsigned long long)b->rate << 32) / VAL_OUT_RATE;
        const int last = b->frames - 1;
        while (f < frames) {
            const int i = (int)(s->pos >> 32);
            if (i > last) break;
            const int frac = (int)((s->pos >> 17) & 0x7FFF); /* 15 bits: diff*frac fits an int */
            const int j = i < last ? i + 1 : last;
            int l, r;
            if (b->channels == 2) {
                const short *p0 = &b->pcm[i * 2], *p1 = &b->pcm[j * 2];
                l = p0[0] + (((p1[0] - p0[0]) * frac) >> 15);
                r = p0[1] + (((p1[1] - p0[1]) * frac) >> 15);
            } else {
                l = b->pcm[i] + (((b->pcm[j] - b->pcm[i]) * frac) >> 15);
                r = l;
            }
            acc[f * 2 + 0] += (l * gq) >> 8;
            acc[f * 2 + 1] += (r * gq) >> 8;
            s->pos += step;
            f++;
        }
        if ((int)(s->pos >> 32) > last) {
            s->pos -= ((unsigned long long)b->frames) << 32;
            s->current++;
        }
    }
    if (s->current >= s->queued) {
        s->state = AL_STOPPED; /* ran dry: everything processed */
        s->pos = 0;
    }
}

static void SDLCALL ValCallback(void *userdata, Uint8 *stream, int len) {
    static int acc[VAL_OUT_FRAMES * 2 * 4];
    short *out = (short *)stream;
    int frames = len / (int)(2 * sizeof(short));
    (void)userdata;
    while (frames > 0) {
        int n = frames > VAL_OUT_FRAMES * 4 ? VAL_OUT_FRAMES * 4 : frames;
        memset(acc, 0, sizeof(int) * (size_t)n * 2);
        for (int i = 0; i < VAL_MAX_SOURCES; i++) {
            ValSource *s = &s_sources[i];
            if (s->used && s->state == AL_PLAYING) ValMixSource(s, acc, n);
        }
        for (int i = 0; i < n * 2; i++) out[i] = (short)ValClamp16(acc[i]);
        out += n * 2;
        frames -= n;
    }
}

/* ---- ALC ---- */

ALCdevice *alcOpenDevice(const ALCchar *devicename) {
    SDL_AudioSpec want, have;
    (void)devicename;
    if (s_dev) return &s_device;
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0 && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[vita-al] SDL audio init failed: %s\n", SDL_GetError());
        return NULL;
    }
    memset(&want, 0, sizeof(want));
    want.freq = VAL_OUT_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = VAL_OUT_FRAMES;
    want.callback = ValCallback;
    s_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!s_dev) {
        fprintf(stderr, "[vita-al] SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return NULL;
    }
    if (have.freq != VAL_OUT_RATE || have.channels != 2 || have.format != AUDIO_S16SYS)
        fprintf(stderr, "[vita-al] device opened as %d Hz, %d ch, fmt 0x%x\n", have.freq, have.channels,
                have.format);
    fprintf(stderr, "[vita-al] audio out %d Hz, %d frames per callback\n", have.freq, have.samples);
    SDL_PauseAudioDevice(s_dev, 0);
    return &s_device;
}

ALCboolean alcCloseDevice(ALCdevice *device) {
    (void)device;
    if (s_dev) {
        SDL_CloseAudioDevice(s_dev);
        s_dev = 0;
    }
    return AL_TRUE;
}

ALCcontext *alcCreateContext(ALCdevice *device, const ALCint *attrlist) {
    (void)attrlist;
    return device ? &s_context : NULL;
}

ALCboolean alcMakeContextCurrent(ALCcontext *context) {
    s_current = context;
    return AL_TRUE;
}

void alcDestroyContext(ALCcontext *context) {
    if (s_current == context) s_current = NULL;
}

ALCcontext *alcGetCurrentContext(void) {
    return s_current;
}

/* ---- buffers ---- */

void alGenBuffers(ALsizei n, ALuint *buffers) {
    ValLock();
    for (ALsizei k = 0; k < n; k++) {
        buffers[k] = 0;
        for (int i = 0; i < VAL_MAX_BUFFERS; i++) {
            if (!s_buffers[i].used) {
                memset(&s_buffers[i], 0, sizeof(s_buffers[i]));
                s_buffers[i].used = 1;
                buffers[k] = (ALuint)(i + 1);
                break;
            }
        }
        if (!buffers[k]) fprintf(stderr, "[vita-al] out of buffers\n");
    }
    ValUnlock();
}

void alDeleteBuffers(ALsizei n, const ALuint *buffers) {
    ValLock();
    for (ALsizei k = 0; k < n; k++) {
        ValBuffer *b = ValBuf(buffers[k]);
        if (!b) continue;
        free(b->pcm);
        memset(b, 0, sizeof(*b));
    }
    ValUnlock();
}

void alBufferData(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei freq) {
    int channels = (format == AL_FORMAT_STEREO16) ? 2 : 1;
    int frames;
    short *pcm;
    if (format != AL_FORMAT_MONO16 && format != AL_FORMAT_STEREO16) return; /* only 16-bit is used */
    frames = size / (int)(sizeof(short) * (size_t)channels);
    pcm = frames > 0 ? (short *)malloc((size_t)frames * (size_t)channels * sizeof(short)) : NULL;
    if (pcm) memcpy(pcm, data, (size_t)frames * (size_t)channels * sizeof(short));
    ValLock();
    {
        ValBuffer *b = ValBuf(buffer);
        if (b) {
            free(b->pcm);
            b->pcm = pcm;
            b->frames = pcm ? frames : 0;
            b->channels = channels;
            b->rate = freq > 0 ? freq : VAL_OUT_RATE;
            pcm = NULL;
        }
    }
    ValUnlock();
    free(pcm);
}

void alBufferiv(ALuint buffer, ALenum param, const ALint *values) {
    /* AL_LOOP_POINTS_SOFT on the VAG uploads: those buffers are never played here. */
    (void)buffer;
    (void)param;
    (void)values;
}

/* ---- sources ---- */

void alGenSources(ALsizei n, ALuint *sources) {
    ValLock();
    for (ALsizei k = 0; k < n; k++) {
        sources[k] = 0;
        for (int i = 0; i < VAL_MAX_SOURCES; i++) {
            if (!s_sources[i].used) {
                memset(&s_sources[i], 0, sizeof(s_sources[i]));
                s_sources[i].used = 1;
                s_sources[i].state = AL_INITIAL;
                s_sources[i].gain = 1.0f;
                sources[k] = (ALuint)(i + 1);
                break;
            }
        }
    }
    ValUnlock();
}

void alDeleteSources(ALsizei n, const ALuint *sources) {
    ValLock();
    for (ALsizei k = 0; k < n; k++) {
        ValSource *s = ValSrc(sources[k]);
        if (s) memset(s, 0, sizeof(*s));
    }
    ValUnlock();
}

void alSourcef(ALuint source, ALenum param, ALfloat value) {
    ValLock();
    {
        ValSource *s = ValSrc(source);
        if (s && param == AL_GAIN) s->gain = value < 0.0f ? 0.0f : value;
    }
    ValUnlock();
}

void alGetSourcei(ALuint source, ALenum param, ALint *value) {
    ValLock();
    {
        ValSource *s = ValSrc(source);
        if (s && value) {
            switch (param) {
            case AL_SOURCE_STATE: *value = s->state; break;
            case AL_BUFFERS_QUEUED: *value = s->queued; break;
            case AL_BUFFERS_PROCESSED:
                *value = (s->state == AL_STOPPED) ? s->queued : s->current;
                break;
            default: break;
            }
        }
    }
    ValUnlock();
}

void alSourceQueueBuffers(ALuint source, ALsizei n, const ALuint *buffers) {
    ValLock();
    {
        ValSource *s = ValSrc(source);
        for (ALsizei k = 0; s && k < n; k++) {
            if (s->queued >= VAL_MAX_QUEUE) {
                fprintf(stderr, "[vita-al] source queue full\n");
                break;
            }
            s->queue[s->queued++] = buffers[k];
        }
    }
    ValUnlock();
}

void alSourceUnqueueBuffers(ALuint source, ALsizei n, ALuint *buffers) {
    ValLock();
    {
        ValSource *s = ValSrc(source);
        if (s) {
            int processed = (s->state == AL_STOPPED) ? s->queued : s->current;
            if (n > processed) n = processed;
            for (ALsizei k = 0; k < n; k++) buffers[k] = s->queue[k];
            memmove(s->queue, s->queue + n, sizeof(ALuint) * (size_t)(s->queued - n));
            s->queued -= n;
            s->current = (s->state == AL_STOPPED) ? s->queued : s->current - n;
            if (s->current < 0) s->current = 0;
        }
    }
    ValUnlock();
}

void alSourcePlay(ALuint source) {
    ValLock();
    {
        ValSource *s = ValSrc(source);
        if (s) {
            if (s->state != AL_PAUSED) { /* stopped/initial: rewind to the start of the queue */
                s->current = 0;
                s->pos = 0;
            }
            s->state = s->queued > 0 ? AL_PLAYING : AL_STOPPED;
        }
    }
    ValUnlock();
}

void alSourceStop(ALuint source) {
    ValLock();
    {
        ValSource *s = ValSrc(source);
        if (s) {
            s->state = AL_STOPPED;
            s->current = s->queued;
            s->pos = 0;
        }
    }
    ValUnlock();
}

void alSourcePause(ALuint source) {
    ValLock();
    {
        ValSource *s = ValSrc(source);
        if (s && s->state == AL_PLAYING) s->state = AL_PAUSED;
    }
    ValUnlock();
}

void alListenerf(ALenum param, ALfloat value) {
    if (param != AL_GAIN) return;
    ValLock();
    s_listenerGain = value < 0.0f ? 0.0f : value;
    ValUnlock();
}

ALenum alGetError(void) {
    return AL_NO_ERROR;
}
