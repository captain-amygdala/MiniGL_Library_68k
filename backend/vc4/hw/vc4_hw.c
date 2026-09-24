/*
 * VideoCore IV (VC4) Low-Level Hardware Control
 */

#include "vc4_hw.h"
#include "vc4_regs.h"
#include "vc4_mock_hw.h"
#include "vc4_debug.h"

static v3d_u8 s_scratch_buffer[512];

void v3d_invalidate_caches(void)
{
    /* On VC4: L2 cache clear bit 2, Slices cache clear 0x0f */
    V3D_L2CACTL = LE32(1 << 2);
    V3D_SLCACTL = LE32(0x0f);
}

v3d_u8 v3d_get_binning_flush_count(void)
{
    return (v3d_u8)(LE32(V3D_BFC) & V3D_BFC_FLUSH_COUNT_MASK);
}

v3d_u8 v3d_get_render_frame_count(void)
{
    return (v3d_u8)(LE32(V3D_RFC) & V3D_RFC_FRAME_COUNT_MASK);
}

v3d_wait_result v3d_wait_for_binning_flush(v3d_u8 lastFlush)
{
    v3d_u8 currentFlushCount;
    v3d_u32 status;

    currentFlushCount = v3d_get_binning_flush_count();
    while (currentFlushCount <= lastFlush && !(currentFlushCount == 0 && lastFlush == 255))
    {
        currentFlushCount = v3d_get_binning_flush_count();
        status = V3D_CT0CS;
        if (status & V3D_CTNCS_CTERR)
            return v3d_wait_result_error_detected;
    }
    return v3d_wait_result_success;
}

v3d_wait_result v3d_wait_for_render_frame(v3d_u8 lastFrame)
{
    v3d_u8 currentFrameCount;
    v3d_u32 status;

    currentFrameCount = v3d_get_render_frame_count();
    while (currentFrameCount <= lastFrame && !(currentFrameCount == 0 && lastFrame == 255))
    {
        currentFrameCount = v3d_get_render_frame_count();
        status = V3D_CT1CS;
        if (status & V3D_CTNCS_CTERR)
            return v3d_wait_result_error_detected;
    }
    return v3d_wait_result_success;
}

void v3d_start_binning_commands(v3d_address binningCommandListStart,
                                v3d_address binningCommandListEnd,
                                v3d_address tileAllocation,
                                v3d_u32 tileAllocationSize,
                                v3d_address tileStateData)
{
    V3D_BPOS = 0;

    if (tileAllocation)
    {
        V3D_BPCA = LE32(tileAllocation);
        V3D_BPCS = LE32(tileAllocationSize);
    }
    if (tileStateData)
    {
        V3D_BPOA = LE32(tileStateData);
    }

    V3D_CT0CA = LE32(binningCommandListStart);
    V3D_CT0EA = LE32(binningCommandListEnd);

    if (v3d_mock_is_active())
    {
        v3d_mock_process_binning(binningCommandListStart, binningCommandListEnd);
    }
}

void v3d_start_render_commands(v3d_address renderCommandListStart, v3d_address renderCommandListEnd)
{
    V3D_CT1CA = LE32(renderCommandListStart);
    V3D_CT1EA = LE32(renderCommandListEnd);

    if (v3d_mock_is_active())
    {
        v3d_mock_process_render(renderCommandListStart, renderCommandListEnd);
    }
}

void* v3d_buffer_claim_memory(v3d_static_buffer* buffer, v3d_uintptr size)
{
    int numBytesFree = buffer->capacity - buffer->used;
    if (numBytesFree >= (int)size)
    {
        void* data = buffer->start + buffer->used;
        buffer->used += size;
        return data;
    }
    buffer->overflowed = 1;
    return (size <= sizeof(s_scratch_buffer)) ? s_scratch_buffer : NULL;
}

v3d_u32 v3d_utile_width(int cpp)
{
    switch (cpp) {
    case 1:  return 8;
    case 2:  return 4;
    case 4:  return 4;
    default: return 4;
    }
}

v3d_u32 v3d_utile_height(int cpp)
{
    switch (cpp) {
    case 1:  return 8;
    case 2:  return 8;
    case 4:  return 4;
    default: return 4;
    }
}
