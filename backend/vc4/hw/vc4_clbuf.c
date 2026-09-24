/*
 * VideoCore IV (VC4) Control-list buffer append and grow
 */

#include "../include/v3d_clbuf.h"
#include "vc4_hw.h"
#include "vc4_debug.h"

#include <exec/execbase.h>
#include <proto/exec.h>

void* v3d_cl_claim_grow(V3DDevice* device, v3d_mem* mem, v3d_static_buffer* buf, v3d_uintptr size, V3DFrame* frame)
{
    v3d_mem new_mem;
    v3d_u32 new_capacity;
    int numBytesFree;

    numBytesFree = buf->capacity - buf->used;
    if (numBytesFree >= (int)size)
    {
        return v3d_buffer_claim_memory(buf, size);
    }

    new_capacity = (v3d_u32)buf->capacity * 2;
    if (new_capacity < (v3d_u32)buf->used + size)
    {
        new_capacity = (v3d_u32)buf->used + (v3d_u32)size;
    }

    D(("vc4: cl_claim_grow: buf=%lx old_cap=%ld used=%ld req=%ld new_cap=%ld\n",
       (ULONG)buf, (LONG)buf->capacity, (LONG)buf->used, (LONG)size, (LONG)new_capacity));

    if (v3d_mem_alloc(device, &new_mem, new_capacity) < 0)
    {
        return 0;
    }
    g_mglv3d_state_buf_grows++;

    {
        struct ExecBase* const SysBase = device->sysbase;
        ULONG len = (ULONG)((buf->used < buf->capacity) ? buf->used : buf->capacity);
        if (len)
            CachePreDMA(buf->start, &len, 0);
    }

    v3d_frame_defer_free(frame, mem);
    *mem = new_mem;

    buf->start = (v3d_u8*)mem->hostptr;
    buf->capacity = (int)mem->size;
    buf->used = 0;

    return v3d_buffer_claim_memory(buf, size);
}

void v3d_cl_reset(v3d_mem* mem, v3d_static_buffer* buf)
{
    buf->used = 0;
    buf->capacity = (int)mem->size;
    buf->overflowed = 0;
}
