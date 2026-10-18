/* Force-included into every translation unit of the PS Vita build (platform/pc/CMakeLists.txt,
 * VH_VITA). Only fills gaps in VitaSDK's newlib that the port's portable code assumes. */
#ifndef VH_VITA_COMPAT_H
#define VH_VITA_COMPAT_H

#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#endif
