/*
 * VideoCore IV (VC4) Memory Allocation via AmigaOS AllocVec
 */

#include <exec/execbase.h>
#include <exec/memory.h>
#include <proto/exec.h>

#include "../include/v3d_device.h"
#include "vc4_debug.h"

#define V3D_ALLOCVEC_ALIGN 64

void v3d_mem_free(V3DDevice* device, v3d_mem* m)
{
    struct ExecBase* const SysBase = device->sysbase;

    if (m->handle)
    {
        device->BytesAllocated -= m->size;
        D(("vc4: AllocVec-freeing %lu bytes, raw = %lx aligned = %lx\n", m->size, m->handle, (ULONG)m->hostptr));
        FreeVec((APTR)m->handle);
    }
    m->size = 0;
    m->handle = 0;
    m->busaddr = 0;
    m->hostptr = 0;
}

int v3d_mem_alloc(V3DDevice* device, v3d_mem* m, unsigned size)
{
    struct ExecBase* const SysBase = device->sysbase;
    APTR raw;
    ULONG aligned;

    m->size = 0;
    m->handle = 0;
    m->busaddr = 0;
    m->hostptr = 0;

    raw = AllocVec(size + V3D_ALLOCVEC_ALIGN, MEMF_PUBLIC | MEMF_REVERSE | MEMF_CLEAR);
    if (!raw)
    {
        D(("vc4: Failed to AllocVec %lu bytes\n", size));
        return -1;
    }

    aligned = ((ULONG)raw + V3D_ALLOCVEC_ALIGN - 1) & ~(V3D_ALLOCVEC_ALIGN - 1);

    m->size = size;
    m->handle = (unsigned)raw;
    m->hostptr = (void*)aligned;
    /* On BCM2837 with Emu68, bus address for un-cached DMA via L2 cache alias */
    m->busaddr = (unsigned)aligned;

    device->BytesAllocated += m->size;
    D(("vc4: AllocVec'd %lu bytes, raw = %lx aligned = %lx\n", size, (ULONG)raw, aligned));
    return 0;
}
