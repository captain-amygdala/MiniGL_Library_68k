/*
 * VideoCore IV (VC4) Submission and Timeout Handlers
 */

#include "vc4_submit_timeout.h"
#include "vc4_regs.h"
#include "vc4_debug.h"

#include <string.h>
#include <exec/execbase.h>
#include <proto/exec.h>

v3d_u32 g_v3d_render_timeout_iterations = V3D_TIMEOUT_ITERATIONS_RENDER;

static int s_binning_validation_failed = 0;

v3d_u8 v3d_submit_binning(V3DDevice* device, V3DFrame* frame,
                          v3d_address binningCommandListStart,
                          v3d_address binningCommandListEnd,
                          v3d_address tileAllocation,
                          v3d_u32 tileAllocationSize,
                          v3d_address tileStateData)
{
    (void)device;
    (void)tileStateData;
    s_binning_validation_failed = 0;

    /* Validate binning control list packets: ONLY valid binning commands allowed! */
    {
        const uint8_t *p = (const uint8_t*)binningCommandListStart;
        const uint8_t *limit = (const uint8_t*)binningCommandListEnd;
        while (p < limit) {
            uint8_t op = *p;
            int len = 0;
            switch (op) {
            case v3d_OP_NOP:                                               len = 1; break;
            case v3d_OP_FLUSH:                                             len = 1; break;
            case v3d_OP_FLUSH_ALL:                                         len = 1; break;
            case v3d_OP_START_TILE_BINNING:                                len = 1; break;
            case v3d_OP_INCREMENT_SEMAPHORE:                               len = 1; break;
            case v3d_OP_WAIT_ON_SEMAPHORE:                                 len = 1; break;
            case v3d_OP_BRANCH:                                            len = 5; break;
            case v3d_OP_BRANCH_TO_SUB_LIST:                                len = 5; break;
            case v3d_OP_RETURN_FROM_SUB_LIST:                              len = 1; break;
            case v3d_OP_INDEXED_PRIM_LIST:                                 len = 14; break;
            case v3d_OP_VERTEX_ARRAY_PRIMS:                                len = 10; break;
            case v3d_OP_PRIM_LIST_FORMAT:                                  len = 2; break;
            case v3d_OP_GL_SHADER_STATE:                                   len = 5; break;
            case v3d_OP_NV_SHADER_STATE:                                   len = 5; break;
            case v3d_OP_CFG_BITS:                                          len = 4; break;
            case v3d_OP_FLAT_SHADE_FLAGS:                                  len = 5; break;
            case v3d_OP_POINT_SIZE:                                        len = 5; break;
            case v3d_OP_LINE_WIDTH:                                        len = 5; break;
            case v3d_OP_RHT_X_BOUNDARY:                                    len = 3; break;
            case v3d_OP_DEPTH_OFFSET:                                      len = 5; break;
            case v3d_OP_CLIPWINDOW:                                        len = 9; break;
            case v3d_OP_VIEWPORT_OFFSET:                                   len = 5; break;
            case v3d_OP_Z_MIN_AND_MAX_CLIPPING_PLANES:                     len = 9; break;
            case v3d_OP_CLIPPER_XY_SCALING:                                len = 9; break;
            case v3d_OP_CLIPPER_Z_SCALE_AND_OFFSET:                        len = 9; break;
            case v3d_OP_TILE_BINNING_MODE_CFG:                             len = 16; break;
            default:                                                       len = -1; break;
            }
            if (len <= 0 || p + len > limit) {
                D(("V3D BINNING VALIDATION ERROR: op=%lu len=%ld at offset %ld (addr=0x%08lx limit=0x%08lx)\n",
                    (ULONG)op, (LONG)len, (LONG)(p - (const uint8_t*)binningCommandListStart), (ULONG)p, (ULONG)limit));
                if (p >= (const uint8_t*)binningCommandListStart + 8 && p + 8 <= limit) {
                    D(("  bytes around error: %02lx %02lx %02lx %02lx %02lx %02lx %02lx %02lx [%02lx] %02lx %02lx %02lx %02lx %02lx %02lx %02lx\n",
                        (ULONG)p[-8], (ULONG)p[-7], (ULONG)p[-6], (ULONG)p[-5], (ULONG)p[-4], (ULONG)p[-3], (ULONG)p[-2], (ULONG)p[-1],
                        (ULONG)p[0], (ULONG)p[1], (ULONG)p[2], (ULONG)p[3], (ULONG)p[4], (ULONG)p[5], (ULONG)p[6], (ULONG)p[7]));
                }
                s_binning_validation_failed = 1;
                return v3d_get_binning_flush_count();
            }
            p += len;
        }
    }

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

    if (s_binning_validation_failed)
    {
        s_binning_validation_failed = 0;
        return v3d_wait_result_error_detected;
    }

    for (;;)
    {
        ULONG intsts;
        ULONG intsts_raw;
        ULONG status;

        currentFlushCount = v3d_get_binning_flush_count();
        if (currentFlushCount != lastFlush)
            break;

        intsts_raw = V3D_CTL_INT_STS;
        intsts = (v3d_u32)LE32(intsts_raw);
        if (intsts & V3D_INT_FLDONE)
            break;
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

        status = LE32(V3D_CT0CS);
        if (status & V3D_CTNCS_CTERR)
        {
            D(("v3d_wait_binning_timeout: CTERR! CT0CS=0x%08lx CT0CA=0x%08lx CT0EA=0x%08lx ERRSTAT=0x%08lx\n",
                status, (ULONG)LE32(V3D_CT0CA), (ULONG)LE32(V3D_CT0EA), (ULONG)LE32(V3D_ERRSTAT)));
            return v3d_wait_result_error_detected;
        }

        iterations++;
        if (iterations >= V3D_TIMEOUT_ITERATIONS_BINNING)
        {
            ULONG bpca = (ULONG)LE32(V3D_BPCA);
            ULONG bpcs = (ULONG)LE32(V3D_BPCS);
            ULONG bpoa = (ULONG)LE32(V3D_BPOA);
            ULONG bpos = (ULONG)LE32(V3D_BPOS);
            UBYTE* ca = (UBYTE*)LE32(V3D_CT0CA);
            D(("v3d_wait_binning_timeout: TIMEOUT! curFlush=%lu lastFlush=%lu CT0CS=0x%08lx CT0CA=0x%08lx CT0EA=0x%08lx\n",
                (ULONG)currentFlushCount, (ULONG)lastFlush, status, (ULONG)ca, (ULONG)LE32(V3D_CT0EA)));
            D(("  BPCA=0x%08lx BPCS=%lu BPOA=0x%08lx BPOS=%lu INTSTS=0x%08lx ERRSTAT=0x%08lx BFC=0x%08lx\n",
                bpca, bpcs, bpoa, bpos, (ULONG)intsts, (ULONG)LE32(V3D_ERRSTAT), (ULONG)V3D_BFC));
            if (ca) {
                D(("  [CA-8..CA+7]: %02x %02x %02x %02x %02x %02x %02x %02x [%02x] %02x %02x %02x %02x %02x %02x %02x\n",
                    ca[-8], ca[-7], ca[-6], ca[-5], ca[-4], ca[-3], ca[-2], ca[-1],
                    ca[0], ca[1], ca[2], ca[3], ca[4], ca[5], ca[6], ca[7]));
            }
            return v3d_wait_result_timed_out;
        }
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
        ULONG status_raw;
        currentFrameCount = v3d_get_render_frame_count();
        if (currentFrameCount != lastFrame)
            break;

        if (LE32(V3D_CTL_INT_STS) & V3D_INT_FRDONE)
            break;

        status_raw = LE32(V3D_CT1CS);
        if (status_raw & V3D_CTNCS_CTERR)
        {
            g_mglv3d_render_wait_iterations = iterations;
            D(("v3d_wait_render_timeout: CTERR detected! CT1CS=0x%08lx CT1CA=0x%08lx CT1EA=0x%08lx ERRSTAT=0x%08lx\n",
                status_raw, (ULONG)LE32(V3D_CT1CA), (ULONG)LE32(V3D_CT1EA), (ULONG)LE32(V3D_ERRSTAT)));
            return v3d_wait_result_error_detected;
        }

        iterations++;
        if (iterations >= g_v3d_render_timeout_iterations)
        {
            g_mglv3d_render_wait_iterations = iterations;
            D(("v3d_wait_render_timeout: TIMEOUT! iter=%lu last=%lu cur=%lu CT1CS=0x%08lx CT1CA=0x%08lx CT1EA=0x%08lx ERRSTAT=0x%08lx RFC=0x%08lx\n",
                (ULONG)iterations, (ULONG)lastFrame, (ULONG)currentFrameCount,
                status_raw, (ULONG)LE32(V3D_CT1CA), (ULONG)LE32(V3D_CT1EA), (ULONG)LE32(V3D_ERRSTAT), (ULONG)V3D_RFC));
            return v3d_wait_result_timed_out;
        }
    }

    g_mglv3d_render_wait_iterations = iterations;
    D(("v3d_wait_render_timeout: success! iter=%lu last=%lu cur=%lu CT1CS=0x%08lx\n",
        (ULONG)iterations, (ULONG)lastFrame, (ULONG)currentFrameCount, (ULONG)LE32(V3D_CT1CS)));
    V3D_CTL_INT_CLR = V3D_CTL_INT_STS;
    return v3d_wait_result_success;
}
