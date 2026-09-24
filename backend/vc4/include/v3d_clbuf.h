/*
 * VideoCore IV (VC4) Growable control-list buffer append
 */

#ifndef VC4_CLBUF_H
#define VC4_CLBUF_H

#include "v3d_types.h"
#include "v3d_device.h"
#include "v3d_frame.h"

void* v3d_cl_claim_grow(V3DDevice* device, v3d_mem* mem, v3d_static_buffer* buf, v3d_uintptr size, V3DFrame* frame);

#ifdef __GNUC__
static __inline__ void* v3d_cl_claim_fast(V3DDevice* device, v3d_mem* mem, v3d_static_buffer* buf, v3d_uintptr size, V3DFrame* frame)
{
    if (buf->capacity - buf->used >= (int)size)
    {
        void* data = buf->start + buf->used;
        buf->used += size;
        return data;
    }
    return v3d_cl_claim_grow(device, mem, buf, size, frame);
}
#endif

void v3d_cl_reset(v3d_mem* mem, v3d_static_buffer* buf);

#endif /* VC4_CLBUF_H */
