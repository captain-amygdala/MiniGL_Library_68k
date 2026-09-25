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
    /* On VC4: L2 cache clear bit 2, Slices cache clear 0x0f0f0f0f */
    V3D_L2CACTL = LE32(1 << 2);
    V3D_SLCACTL = LE32(0x0f0f0f0f);
}

v3d_u8 v3d_get_binning_flush_count(void)
{
    uint32_t raw = V3D_BFC;
    return (v3d_u8)((raw & 0xFF) | ((raw >> 24) & 0xFF));
}

v3d_u8 v3d_get_render_frame_count(void)
{
    uint32_t raw = V3D_RFC;
    return (v3d_u8)((raw & 0xFF) | ((raw >> 24) & 0xFF));
}

v3d_wait_result v3d_wait_for_binning_flush(v3d_u8 lastFlush)
{
    v3d_u8 currentFlushCount;

    currentFlushCount = v3d_get_binning_flush_count();
    while (currentFlushCount == lastFlush)
    {
        currentFlushCount = v3d_get_binning_flush_count();
        if (LE32(V3D_CT0CS) & 0x08)
            return v3d_wait_result_error_detected;
    }
    return v3d_wait_result_success;
}

v3d_wait_result v3d_wait_for_render_frame(v3d_u8 lastFrame)
{
    v3d_u8 currentFrameCount;

    currentFrameCount = v3d_get_render_frame_count();
    while (currentFrameCount == lastFrame)
    {
        currentFrameCount = v3d_get_render_frame_count();
        if (LE32(V3D_CT1CS) & 0x08)
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
    (void)tileAllocation;
    (void)tileAllocationSize;
    (void)tileStateData;
    V3D_BPOS = 0;

    /* Ensure thread 0 is stopped and reset (Broadcom Table 49: CTRSTA) */
    if (LE32(V3D_CT0CS) & (0x20 | 0x10 | 0x08))
    {
        V3D_CT0CS = LE32(0x8000);
        int spin = 10000;
        while ((LE32(V3D_CT0CS) & 0x20) && --spin > 0) {}
    }
    V3D_CTL_INT_CLR = LE32(0xFFFFFFFF);

    D(("v3d_start_binning_commands: CT0CA=0x%08lx CT0EA=0x%08lx\n",
        (ULONG)binningCommandListStart, (ULONG)binningCommandListEnd));

    V3D_CT0CA = LE32(binningCommandListStart);
    V3D_CT0EA = LE32(binningCommandListEnd);

    if (v3d_mock_is_active())
    {
        v3d_mock_process_binning(binningCommandListStart, binningCommandListEnd);
    }
}

void v3d_start_render_commands(v3d_address renderCommandListStart, v3d_address renderCommandListEnd)
{
    /* Ensure thread 1 is stopped and reset (Broadcom Table 49: CTRSTA) */
    if (LE32(V3D_CT1CS) & (0x20 | 0x10 | 0x08))
    {
        V3D_CT1CS = LE32(0x8000);
        int spin = 10000;
        while ((LE32(V3D_CT1CS) & 0x20) && --spin > 0) {}
    }
    V3D_CTL_INT_CLR = LE32(0xFFFFFFFF);

    D(("v3d_start_render_commands: CT1CA=0x%08lx CT1EA=0x%08lx len=%lu\n",
        (ULONG)renderCommandListStart, (ULONG)renderCommandListEnd,
        (ULONG)(renderCommandListEnd - renderCommandListStart)));

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
