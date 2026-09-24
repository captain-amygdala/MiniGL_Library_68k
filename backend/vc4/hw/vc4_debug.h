/*
 * VideoCore IV (VC4) Debug definitions
 */

#ifndef VC4_DEBUG_H
#define VC4_DEBUG_H

#include <exec/types.h>

void kprintf(STRPTR format, ...);

#ifdef DEBUG
#define D(x) do { kprintf x; } while (0)
#else
#define D(x) do { } while (0)
#endif

#define E(x) do { kprintf x; } while (0)

#endif /* VC4_DEBUG_H */
