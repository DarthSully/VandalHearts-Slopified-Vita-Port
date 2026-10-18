/* PS Vita platform glue for the Vandal Hearts port.
 *
 * Runs before everything else (constructor priority 100, ahead of pc_bootstrap.c's 101 ini
 * loader): CPU/GPU clocks, the data folder (made the working directory, so every relative path the
 * port uses lands there), and the log file. See platform/vita/README.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <psp2/apputil.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/touch.h>

#ifndef VH_VITA_DATA_DIR
#define VH_VITA_DATA_DIR "ux0:data/vandalhearts"
#endif
#ifndef VH_VITA_BUILD
#define VH_VITA_BUILD "dev"
#endif

/* newlib heap. The port keeps two 1 MB VRAM images, the decoded VAB samples, the hi-res raster
 * target when supersampling is on, and reads the disc a sector at a time; 192 MB leaves room for
 * an HD pack later. Extended memory mode (param.sfo ATTRIBUTE2=12) makes this fit. */
int _newlib_heap_size_user = 192 * 1024 * 1024;

/* Main thread stack: decompiled PS1 code keeps sizeable locals. */
unsigned int sceUserMainThreadStackSize = 2 * 1024 * 1024;

/* Default stack for attr-less pthreads (the hi-res raster workers, the HD-pack loader). The
 * pthread-embedded default is 32 KB. */
unsigned int _pthread_stack_default_user = 1024 * 1024;

static void VitaMkdirP(const char *path) {
    char buf[256];
    size_t n = strlen(path);
    if (n >= sizeof(buf)) return;
    memcpy(buf, path, n + 1);
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == '/' && i > 0 && buf[i - 1] != ':') {
            buf[i] = '\0';
            sceIoMkdir(buf, 0777);
            buf[i] = '/';
        }
    }
    sceIoMkdir(buf, 0777);
}

static void VitaFlushLog(void) {
    fflush(stderr);
}

static int VitaFileExists(const char *name) {
    FILE *f = fopen(name, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* Rear touchpad as the missing L2/R2 (libetc.c PadRead): bit 0 = a touch on the left half,
 * bit 1 = a touch on the right half. The rear panel reports 0..1919 horizontally. */
int PC_VitaRearTouch(void) {
    static int s_init = 0;
    SceTouchData td;
    int bits = 0;
    if (!s_init) {
        sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
        s_init = 1;
    }
    if (sceTouchPeek(SCE_TOUCH_PORT_BACK, &td, 1) < 1) return 0;
    for (unsigned i = 0; i < td.reportNum; i++)
        bits |= (td.report[i].x < 960) ? 1 : 2;
    return bits;
}

#pragma GCC diagnostic ignored "-Wprio-ctor-dtor"
__attribute__((constructor(100))) static void VitaEarlyInit(void) {
    SceAppUtilInitParam init;
    SceAppUtilBootParam boot;

    /* Max clocks: CPU 444 MHz, bus 222, GPU 222, crossbar 166. */
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    memset(&init, 0, sizeof(init));
    memset(&boot, 0, sizeof(boot));
    sceAppUtilInit(&init, &boot);

    VitaMkdirP(VH_VITA_DATA_DIR);
    VitaMkdirP(VH_VITA_DATA_DIR "/game");
    if (chdir(VH_VITA_DATA_DIR) != 0) {
        /* Carry on: the disc check reports the missing file with the path it tried. */
    }

    /* One log file, one FILE handle for both streams (two handles on the same file overwrite each
     * other). Line-buffered so the last line before a hang is on disk. */
    if (freopen(VH_VITA_DATA_DIR "/vandalhearts.log", "w", stderr)) {
        setvbuf(stderr, NULL, _IOLBF, 0);
        stdout = stderr;
    }
    atexit(VitaFlushLog);

    /* Main thread on core 0; workers the port creates float on the other user cores. */
    sceKernelChangeThreadCpuAffinityMask(sceKernelGetThreadId(), SCE_KERNEL_CPU_MASK_USER_0);

    fprintf(stderr, "[vita] Vandal Hearts PC port, Vita build " VH_VITA_BUILD " -- data dir: " VH_VITA_DATA_DIR
                    "\n");

    /* Switch files in the data folder (empty files). */
    if (VitaFileExists("profile")) {
        setenv("VH_PRESENT_TIME", "1", 0);
        setenv("VH_RASTER_TIME", "1", 0);
        fprintf(stderr, "[vita] profile: present/raster timing on\n");
    }
}
