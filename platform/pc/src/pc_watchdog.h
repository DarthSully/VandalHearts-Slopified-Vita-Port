/* Hang breadcrumbs for the PS Vita watchdog (platform/vita/src/vita_watchdog.c). On other platforms
 * every macro compiles to nothing. Stages are string literals; the watchdog only reads them. */
#ifndef PC_WATCHDOG_H
#define PC_WATCHDOG_H

#ifdef __vita__
extern const char *volatile g_vhWdStage;        /* last stage the main thread entered */
extern volatile unsigned g_vhWdFrames;          /* VSync calls completed */
extern const char *volatile g_vhWdWorker[8];    /* per raster-worker stage */
extern volatile int g_vhWdWorkerChunk[8];       /* chunk each worker is drawing, -1 when none */
extern volatile unsigned g_vhWdSerialFrames;    /* frames drawn serially (texture/draw-area overlap) */
extern volatile unsigned g_vhWdLastSerialFrame; /* g_vhWdFrames at the latest serial frame */
#define VH_WD_STAGE(s) (g_vhWdStage = (s))
#define VH_WD_FRAME_DONE() (g_vhWdFrames++)
#define VH_WD_WORKER(id, s, chunk) \
    do { if ((unsigned)(id) < 8) { g_vhWdWorker[id] = (s); g_vhWdWorkerChunk[id] = (chunk); } } while (0)
#define VH_WD_SERIAL_FRAME() (g_vhWdSerialFrames++, g_vhWdLastSerialFrame = g_vhWdFrames)
#else
#define VH_WD_STAGE(s) ((void)0)
#define VH_WD_FRAME_DONE() ((void)0)
#define VH_WD_WORKER(id, s, chunk) ((void)0)
#define VH_WD_SERIAL_FRAME() ((void)0)
#endif

#endif
