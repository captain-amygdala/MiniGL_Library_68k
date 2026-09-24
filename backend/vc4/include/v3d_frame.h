/*
 * VideoCore IV (VC4) Frame and pool management
 */

#ifndef VC4_FRAME_H
#define VC4_FRAME_H

#include "v3d_types.h"
#include "v3d_device.h"

#define V3D_SPILL_BLOCK_SIZE (256 * 1024)
#define V3D_MAX_SPILL_BLOCKS 8

typedef struct V3DFrame {
    v3d_u32 tilesX, tilesY;
    v3d_mem spill_blocks[V3D_MAX_SPILL_BLOCKS];
    int spill_count;
} V3DFrame;

extern v3d_u32 g_mglv3d_bin_spills;
extern v3d_u32 g_mglv3d_bin_spill_fails;
extern v3d_u32 g_mglv3d_state_buf_grows;
extern v3d_u32 g_mglv3d_frames_dropped;
extern v3d_u32 g_mglv3d_retire_leaks;
extern int     g_v3d_requested_zbuffer_bits;

void v3d_frame_compute_pool_sizes(v3d_u16 width, v3d_u16 height,
                                   int zbuffer_bits,
                                   v3d_u32* zbuffer_size,
                                   v3d_u32* tile_state_size,
                                   v3d_u32* tile_alloc_size,
                                   v3d_u32* tilesX,
                                   v3d_u32* tilesY);

void v3d_frame_begin(V3DFrame* frame, v3d_u32 tilesX, v3d_u32 tilesY);
void v3d_frame_end(V3DDevice* device, V3DFrame* frame);
void v3d_frame_binning_job_begin(V3DFrame* frame);
void v3d_frame_render_submitted(V3DFrame* frame);
void v3d_frame_retire(V3DDevice* device, V3DFrame* frame);
v3d_mem* v3d_frame_add_spill_block(V3DDevice* device, V3DFrame* frame);
void v3d_frame_defer_free(V3DFrame* frame, v3d_mem* mem);

#endif /* VC4_FRAME_H */
