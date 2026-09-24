/*
 * VideoCore IV (VC4) Submission and Timeout Handlers
 */

#include "vc4_submit_timeout.h"
#include "vc4_regs.h"
#include "vc4_debug.h"

v3d_u32 g_v3d_render_timeout_iterations = V3D_TIMEOUT_ITERATIONS_RENDER;

v3d_u8 v3d_submit_binning(V3DDevice* device, V3DFrame* frame,
                          v3d_address binningCommandListStart,
                          v3d_address binningCommandListEnd,
                          v3d_address tileAllocation,
                          v3d_u32 tileAllocationSize,
                          v3d_address tileStateData)
{
    (void)device;
    v3d_u8 lastFlush = v3d_get_binning_flush_count();
    v3d_frame_binning_job_begin(frame);
    v3d_start_binning_commands(binningCommandListStart, binningCommandListEnd,
                                tileAllocation, tileAllocationSize, tileStateData);
    return lastFlush;
}

v3d_wait_result v3d_wait_binning_timeout(V3DDevice* device, V3DFrame* frame, v3d_u8 lastFlush)
{
    v3d_u8 currentFlushCount;
    v3d_u32 iterations = 0;

    for (;;)
    {
        ULONG intsts;
        ULONG intsts_raw;
        ULONG status;

        currentFlushCount = v3d_get_binning_flush_count();
        if (currentFlushCount > lastFlush || (currentFlushCount == 0 && lastFlush == 255))
            break;

        intsts_raw = V3D_CTL_INT_STS;
        intsts = (v3d_u32)LE32(intsts_raw);
        if (intsts & V3D_INT_OUTOMEM)
        {
            v3d_mem* spill;
            V3D_CTL_INT_CLR = intsts_raw;

            spill = v3d_frame_add_spill_block(device, frame);
            if (!spill)
                return v3d_wait_result_error_detected;

            V3D_BPOA = LE32(spill->busaddr);
            V3D_BPOS = LE32(V3D_SPILL_BLOCK_SIZE);
            continue;
        }

        status = V3D_CT0CS;
        if (status & V3D_CTNCS_CTERR)
            return v3d_wait_result_error_detected;

        iterations++;
        if (iterations >= V3D_TIMEOUT_ITERATIONS_BINNING)
            return v3d_wait_result_timed_out;
    }

    V3D_CTL_INT_CLR = V3D_CTL_INT_STS;
    return v3d_wait_result_success;
}

v3d_u8 v3d_submit_render(v3d_address renderCommandListStart,
                         v3d_address renderCommandListEnd)
{
    v3d_u8 lastFrame = v3d_get_render_frame_count();
    v3d_start_render_commands(renderCommandListStart, renderCommandListEnd);
    return lastFrame;
}

v3d_u32 g_mglv3d_render_wait_iterations = 0;

v3d_wait_result v3d_wait_render_timeout(v3d_u8 lastFrame)
{
    v3d_u8 currentFrameCount;
    v3d_u32 iterations = 0;

    for (;;)
    {
        ULONG status;
        currentFrameCount = v3d_get_render_frame_count();
        if (currentFrameCount > lastFrame || (currentFrameCount == 0 && lastFrame == 255))
            break;

        status = V3D_CT1CS;
        if (status & V3D_CTNCS_CTERR)
        {
            g_mglv3d_render_wait_iterations = iterations;
            return v3d_wait_result_error_detected;
        }

        iterations++;
        if (iterations >= g_v3d_render_timeout_iterations)
        {
            g_mglv3d_render_wait_iterations = iterations;
            return v3d_wait_result_timed_out;
        }
    }

    g_mglv3d_render_wait_iterations = iterations;
    V3D_CTL_INT_CLR = V3D_CTL_INT_STS;
    return v3d_wait_result_success;
}
