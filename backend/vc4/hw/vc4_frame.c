/*
 * VideoCore IV (VC4) Frame and Tile Pool Calculation
 */

#include "../include/v3d_frame.h"
#include "vc4_hw.h"
#include "vc4_debug.h"

#include <exec/execbase.h>
#include <proto/exec.h>

static ULONG divRoundUp(ULONG a, ULONG b)
{
    return (a + b - 1) / b;
}

int g_v3d_requested_zbuffer_bits = 32;
v3d_u32 g_v3d_cl_generation = 0;

void v3d_frame_compute_pool_sizes(v3d_u16 width, v3d_u16 height,
                                   int zbuffer_bits,
                                   v3d_u32* zbuffer_size,
                                   v3d_u32* tile_state_size,
                                   v3d_u32* tile_alloc_size,
                                   v3d_u32* tilesX,
                                   v3d_u32* tilesY)
{
    /* VC4 tile size: 64x64 in 32-bit mode */
    const v3d_u32 tile_width = 64;
    const v3d_u32 tile_height = 64;
    const v3d_u32 tsda_per_tile_size = 64; /* On VC4 (V3D 2.x), TSDA is 64 bytes per tile! */
    v3d_u32 size;

    *tilesX = divRoundUp(width, tile_width);
    *tilesY = divRoundUp(height, tile_height);

    *zbuffer_size = V3D_ALIGN_UP(width, 64) * V3D_ALIGN_UP(height, 64)
                    * (v3d_u32)((zbuffer_bits == 16) ? 2 : 4);

    *tile_state_size = (*tilesY) * (*tilesX) * tsda_per_tile_size;

    size = (*tilesX) * (*tilesY) * 64;
    size = V3D_ALIGN_UP(size, 4096);
    size += 8192;
    size += 512 * 1024;
    *tile_alloc_size = size;
}

v3d_u32 g_mglv3d_bin_spills = 0;
v3d_u32 g_mglv3d_bin_spill_fails = 0;
v3d_u32 g_mglv3d_state_buf_grows = 0;
v3d_u32 g_mglv3d_frames_dropped = 0;
v3d_u32 g_mglv3d_retire_leaks = 0;

#define V3D_RETIRE_MAX 32
typedef struct {
    v3d_mem mem;
    v3d_u32 seq;
    int     is_spill;
} v3d_retire_entry;

static v3d_retire_entry s_retire[V3D_RETIRE_MAX];
static int     s_retire_count = 0;
static v3d_u32 s_build_seq = 1;
static v3d_u32 s_render_seq = 0;
static int     s_job_spills = 0;

void v3d_frame_begin(V3DFrame* frame, v3d_u32 tilesX, v3d_u32 tilesY)
{
    frame->tilesX = tilesX;
    frame->tilesY = tilesY;
    frame->spill_count = 0;
}

void v3d_frame_end(V3DDevice* device, V3DFrame* frame)
{
    for (int i = 0; i < s_retire_count; i++)
        v3d_mem_free(device, &s_retire[i].mem);
    s_retire_count = 0;

    for (int i = 0; i < frame->spill_count; i++)
        v3d_mem_free(device, &frame->spill_blocks[i]);
    frame->spill_count = 0;
}

void v3d_frame_binning_job_begin(V3DFrame* frame)
{
    (void)frame;
    s_job_spills = 0;
}

void v3d_frame_render_submitted(V3DFrame* frame)
{
    (void)frame;
    s_render_seq = s_build_seq++;
}

void v3d_frame_retire(V3DDevice* device, V3DFrame* frame)
{
    int write = 0;
    for (int read = 0; read < s_retire_count; read++)
    {
        if (s_retire[read].seq <= s_render_seq)
        {
            if (s_retire[read].is_spill && frame->spill_count < V3D_MAX_SPILL_BLOCKS)
            {
                frame->spill_blocks[frame->spill_count++] = s_retire[read].mem;
            }
            else
            {
                v3d_mem_free(device, &s_retire[read].mem);
            }
        }
        else
        {
            if (write != read)
                s_retire[write] = s_retire[read];
            write++;
        }
    }
    s_retire_count = write;
}

v3d_mem* v3d_frame_add_spill_block(V3DDevice* device, V3DFrame* frame)
{
    v3d_mem block;
    if (s_job_spills >= V3D_MAX_SPILL_BLOCKS || s_retire_count >= V3D_RETIRE_MAX)
    {
        g_mglv3d_bin_spill_fails++;
        return NULL;
    }

    if (frame->spill_count > 0)
    {
        block = frame->spill_blocks[--frame->spill_count];
    }
    else
    {
        if (v3d_mem_alloc(device, &block, V3D_SPILL_BLOCK_SIZE + 4096) < 0)
        {
            g_mglv3d_bin_spill_fails++;
            return NULL;
        }
        {
            struct ExecBase* const SysBase = device->sysbase;
            ULONG len = (ULONG)block.size;
            CachePreDMA(block.hostptr, &len, 0);
        }
        block.busaddr = V3D_ALIGN_UP(block.busaddr, 4096);
        block.hostptr = (void*)V3D_ALIGN_UP((v3d_uintptr)block.hostptr, 4096);
    }

    s_retire[s_retire_count].mem = block;
    s_retire[s_retire_count].seq = s_build_seq;
    s_retire[s_retire_count].is_spill = 1;
    s_retire_count++;
    s_job_spills++;
    g_mglv3d_bin_spills++;

    return &s_retire[s_retire_count - 1].mem;
}

void v3d_frame_defer_free(V3DFrame* frame, v3d_mem* mem)
{
    (void)frame;
    if (s_retire_count < V3D_RETIRE_MAX)
    {
        s_retire[s_retire_count].mem = *mem;
        s_retire[s_retire_count].seq = s_build_seq;
        s_retire[s_retire_count].is_spill = 0;
        s_retire_count++;
    }
    else
    {
        g_mglv3d_retire_leaks++;
    }
    mem->size = 0;
    mem->handle = 0;
    mem->busaddr = 0;
    mem->hostptr = 0;
}
