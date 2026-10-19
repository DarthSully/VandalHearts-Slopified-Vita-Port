/* Hang watchdog for the PS Vita build: a thread polls the main thread's VSync count and, after a
 * stall, writes the breadcrumbs (pc_watchdog.h) to hang_report.txt and faults on purpose so the
 * Vita writes a psp2core dump of every thread. Design notes in platform/vita/HANDOFF.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

const char *volatile g_vhWdStage = "boot";
volatile unsigned g_vhWdFrames = 0;
const char *volatile g_vhWdWorker[8];
volatile int g_vhWdWorkerChunk[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
volatile unsigned g_vhWdSerialFrames = 0;
volatile unsigned g_vhWdLastSerialFrame = 0;

extern int PC_RasterPoolDebug(char *out, int cap);   /* pc_raster.c, lock-free read */
extern int PC_GpuAsyncInFlight(void);               /* libgpu.c */

enum { kPollUs = 250000, kBootPolls = 80 };

static int s_runPolls = 24;

static void WdWrite(SceUID fd, const char *s) {
    if (fd >= 0) sceIoWrite(fd, s, strlen(s));
}

static void WdReport(unsigned frames, int polls) {
    /* The main thread may be stuck holding stderr's lock, so this writes through sceIo only. */
    static char buf[2048];
    char pool[256];
    int n, i;
    pool[0] = '\0';
    PC_RasterPoolDebug(pool, (int)sizeof(pool));
    n = snprintf(buf, sizeof(buf),
                 "\n[watchdog] HANG: no VSync for %.2f s (frames=%u)\n"
                 "[watchdog] main thread stage: %s\n"
                 "[watchdog] async raster in flight: %d\n"
                 "[watchdog] serial frames: %u (latest at frame %u)\n"
                 "[watchdog] pool: %s\n",
                 polls * (kPollUs / 1e6), frames, g_vhWdStage ? g_vhWdStage : "?",
                 PC_GpuAsyncInFlight(), g_vhWdSerialFrames, g_vhWdLastSerialFrame, pool);
    for (i = 0; i < 8 && n > 0 && n < (int)sizeof(buf); i++) {
        if (!g_vhWdWorker[i]) continue;
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, i ? "[watchdog] worker %d: %s (chunk %d)\n"
                        : "[watchdog] main thread's own drawing (%d): last %s (chunk %d)\n",
                      i, g_vhWdWorker[i], g_vhWdWorkerChunk[i]);
    }
    if (n > 0 && n < (int)sizeof(buf))
        snprintf(buf + n, sizeof(buf) - (size_t)n,
                 "[watchdog] faulting on purpose: send this file and the newest psp2core dump\n");
    {
        SceUID fd = sceIoOpen("ux0:data/vandalhearts/hang_report.txt",
                              SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
        WdWrite(fd, buf);
        if (fd >= 0) sceIoClose(fd);
        fd = sceIoOpen("ux0:data/vandalhearts/vandalhearts.log", SCE_O_WRONLY, 0777);
        if (fd >= 0) sceIoLseek(fd, 0, SCE_SEEK_END);   /* add to the end; never overwrite the log */
        WdWrite(fd, buf);
        if (fd >= 0) sceIoClose(fd);
    }
}

static int WdThread(SceSize args, void *argp) {
    unsigned last = g_vhWdFrames;
    int still = 0;
    (void)args;
    (void)argp;
    for (;;) {
        sceKernelDelayThread(kPollUs);
        if (g_vhWdFrames != last) {
            if (last == 0) {   /* first frame: the .ini (loaded after this thread starts) is in */
                const char *secs = getenv("VH_WATCHDOG_SECONDS");
                if (secs && atoi(secs) >= 2) s_runPolls = atoi(secs) * 4;
            }
            last = g_vhWdFrames;
            still = 0;
            continue;
        }
        if (++still < (last == 0 ? kBootPolls : s_runPolls)) continue;
        WdReport(last, still);
        sceKernelDelayThread(200000);
        *(volatile unsigned *)8 = 0xDEADu;   /* deliberate fault -> psp2core dump */
        still = 0;
    }
    return 0;
}

/* Started from the platform constructor (vita_platform.c) once the data folder exists. */
void PC_VitaWatchdogStart(void) {
    FILE *f = fopen("no_watchdog", "rb");
    SceUID tid;
    if (f) {
        fclose(f);
        fprintf(stderr, "[watchdog] off (no_watchdog file)\n");
        return;
    }
    tid = sceKernelCreateThread("vh_watchdog", WdThread, 100, 0x4000, 0, SCE_KERNEL_CPU_MASK_USER_2, NULL);
    if (tid < 0 || sceKernelStartThread(tid, 0, NULL) < 0) {
        fprintf(stderr, "[watchdog] could not start (0x%08x)\n", (unsigned)tid);
        return;
    }
    fprintf(stderr, "[watchdog] armed: 6 s without a frame (20 s at boot; VH_WATCHDOG_SECONDS)\n");
}
