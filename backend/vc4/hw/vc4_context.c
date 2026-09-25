/*
 * VideoCore IV (VC4) Context Management
 */

#include "../include/v3d_context.h"
#include "vc4_debug.h"
#include "vc4_submit_timeout.h"

#include <exec/execbase.h>
#include <proto/exec.h>

static void align_mem_4096(V3DDevice* device, v3d_mem* m)
{
    v3d_uintptr raw     = (v3d_uintptr)m->hostptr;
    v3d_uintptr aligned = V3D_ALIGN_UP(raw, 4096);
    v3d_u32     delta   = (v3d_u32)(aligned - raw);

    m->busaddr = V3D_ALIGN_UP(m->busaddr, 4096);
    m->hostptr = (void*)aligned;

    if (m->size > delta)
        m->size -= delta;
    else
        m->size = 0;

    if (device)
        device->BytesAllocated -= delta;
}

static void free_partial(V3DDevice* device, V3DContext* context, int stage)
{
    if (stage > 10) v3d_mem_free(device, &context->state_mem[1]);
    if (stage > 9)  v3d_mem_free(device, &context->state_mem[0]);
    if (stage > 8)  v3d_mem_free(device, &context->tile_list_mem[1]);
    if (stage > 7)  v3d_mem_free(device, &context->tile_list_mem[0]);
    if (stage > 6)  v3d_mem_free(device, &context->render_mem[1]);
    if (stage > 5)  v3d_mem_free(device, &context->render_mem[0]);
    if (stage > 4)  v3d_mem_free(device, &context->binning_mem[1]);
    if (stage > 3)  v3d_mem_free(device, &context->binning_mem[0]);
    if (stage > 2)  v3d_mem_free(device, &context->tile_alloc_mem);
    if (stage > 1)  v3d_mem_free(device, &context->tile_state_mem);
    if (stage > 0)  v3d_mem_free(device, &context->zbuffer_mem);
}

int v3d_context_init(V3DContext* context, V3DDevice* device, v3d_u16 width, v3d_u16 height)
{
    v3d_u32 zbuffer_size, tile_state_size, tile_alloc_size, tilesX, tilesY;

    context->framebuffer = 0;
    context->width = width;
    context->height = height;
    context->y_offset = 0;
    context->render_mode_cfg_pending = 0;
    context->clear_colors_emitted = 0;

    g_v3d_render_timeout_iterations = V3D_TIMEOUT_ITERATIONS_RENDER;

    D(("vc4_context_init: enter, width=%ld height=%ld timeout_iter=%lu\n",
        (LONG)width, (LONG)height, (ULONG)g_v3d_render_timeout_iterations));

    context->zbuffer_bits = (g_v3d_requested_zbuffer_bits == 16) ? 16 : 32;

    v3d_frame_compute_pool_sizes(width, height, context->zbuffer_bits,
                                  &zbuffer_size, &tile_state_size,
                                  &tile_alloc_size, &tilesX, &tilesY);

    if (v3d_mem_alloc(device, &context->zbuffer_mem, zbuffer_size + 4096) < 0)
        return -1;
    {
        ULONG len = (ULONG)context->zbuffer_mem.size;
        CachePreDMA(context->zbuffer_mem.hostptr, &len, 0);
    }
    align_mem_4096(device, &context->zbuffer_mem);

    if (v3d_mem_alloc(device, &context->tile_state_mem, tile_state_size + 4096) < 0)
    {
        free_partial(device, context, 1);
        return -2;
    }
    {
        ULONG len = (ULONG)context->tile_state_mem.size;
        CachePreDMA(context->tile_state_mem.hostptr, &len, 0);
    }
    align_mem_4096(device, &context->tile_state_mem);

    if (v3d_mem_alloc(device, &context->tile_alloc_mem, tile_alloc_size + 4096) < 0)
    {
        free_partial(device, context, 2);
        return -3;
    }
    {
        ULONG len = (ULONG)context->tile_alloc_mem.size;
        CachePreDMA(context->tile_alloc_mem.hostptr, &len, 0);
    }
    align_mem_4096(device, &context->tile_alloc_mem);

    if (v3d_mem_alloc(device, &context->binning_mem[0], V3D_INITIAL_CL_BUFFER_SIZE) < 0)
    {
        free_partial(device, context, 3);
        return -4;
    }
    if (v3d_mem_alloc(device, &context->binning_mem[1], V3D_INITIAL_CL_BUFFER_SIZE) < 0)
    {
        free_partial(device, context, 4);
        return -4;
    }

    if (v3d_mem_alloc(device, &context->render_mem[0], V3D_INITIAL_RENDER_BUFFER_SIZE) < 0)
    {
        free_partial(device, context, 5);
        return -5;
    }
    if (v3d_mem_alloc(device, &context->render_mem[1], V3D_INITIAL_RENDER_BUFFER_SIZE) < 0)
    {
        free_partial(device, context, 6);
        return -5;
    }

    if (v3d_mem_alloc(device, &context->tile_list_mem[0], V3D_INITIAL_TILE_LIST_BUFFER_SIZE) < 0)
    {
        free_partial(device, context, 7);
        return -6;
    }
    if (v3d_mem_alloc(device, &context->tile_list_mem[1], V3D_INITIAL_TILE_LIST_BUFFER_SIZE) < 0)
    {
        free_partial(device, context, 8);
        return -6;
    }

    if (v3d_mem_alloc(device, &context->state_mem[0], V3D_INITIAL_CL_BUFFER_SIZE) < 0)
    {
        free_partial(device, context, 9);
        return -7;
    }
    if (v3d_mem_alloc(device, &context->state_mem[1], V3D_INITIAL_CL_BUFFER_SIZE) < 0)
    {
        free_partial(device, context, 10);
        return -7;
    }

    for (int i = 0; i < 2; i++) {
        context->binning_buf[i].start = (v3d_u8*)context->binning_mem[i].hostptr;
        context->binning_buf[i].used = 0;
        context->binning_buf[i].capacity = V3D_INITIAL_CL_BUFFER_SIZE;
        context->binning_buf[i].overflowed = 0;

        context->render_buf[i].start = (v3d_u8*)context->render_mem[i].hostptr;
        context->render_buf[i].used = 0;
        context->render_buf[i].capacity = V3D_INITIAL_RENDER_BUFFER_SIZE;
        context->render_buf[i].overflowed = 0;

        context->tile_list_buf[i].start = (v3d_u8*)context->tile_list_mem[i].hostptr;
        context->tile_list_buf[i].used = 0;
        context->tile_list_buf[i].capacity = V3D_INITIAL_TILE_LIST_BUFFER_SIZE;
        context->tile_list_buf[i].overflowed = 0;

        context->state_buf[i].start = (v3d_u8*)context->state_mem[i].hostptr;
        context->state_buf[i].used = 0;
        context->state_buf[i].capacity = V3D_INITIAL_CL_BUFFER_SIZE;
        context->state_buf[i].overflowed = 0;
    }

    context->current_buf = 0;
    context->build_slot = 0;
    context->render_pending = 0;
    context->render_lastframe = 0;

    context->ez_state = 0;
    context->first_ez_state = 0;
    context->depth_ez_dir = 0;
    context->depth_persist_seen = 0;

    context->force_new_pass = 0;
    context->doing_intermediate_pass = 0;
    context->has_scratch_color = 0;
    memset(&context->scratch_color_mem, 0, sizeof(context->scratch_color_mem));
    context->scratch_stride = 0;

    v3d_frame_begin(&context->frame, tilesX, tilesY);

    context->fixed_color = 0xFFFFFFFF;
    context->zmode = V3D_Z_LESS;
    context->blend_srcmode = V3D_BLEND_FACTOR_ONE;
    context->blend_dstmode = V3D_BLEND_FACTOR_ZERO;
    context->blend_alpha_srcmode = V3D_BLEND_FACTOR_ONE;
    context->blend_alpha_dstmode = V3D_BLEND_FACTOR_ZERO;
    context->blend_color_equation = 0;
    context->blend_alpha_equation = 0;
    context->blend_nonadd_warned = 0;
    context->alpha_test_enable = 0;
    context->alpha_func = V3D_ALPHAFUNC_ALWAYS;
    context->alpha_ref = 0.0f;
    context->color_mask_r = 1;
    context->color_mask_g = 1;
    context->color_mask_b = 1;
    context->color_mask_a = 1;

    context->scissor_enable = 0;
    context->scissor_x = 0;
    context->scissor_y = 0;
    context->scissor_w = width;
    context->scissor_h = height;

    context->fog_enable = 0;
    context->fog_mode = V3D_FOG_LINEAR;
    context->fog_start = 0.0f;
    context->fog_end = 1.0f;
    context->fog_density = 1.0f;
    context->fog_r = 0;
    context->fog_g = 0;
    context->fog_b = 0;
    context->fog_a = 0;

    for (int t = 0; t < V3D_MAX_TEXUNIT; t++)
        context->bound_texture[t] = 0;
    context->texture_list = 0;

    context->frame_active = 0;
    context->pending_clear_color = 0;
    context->pending_clear_depth = 0;
    context->frame_corrupted = 0;

    context->clear_color_value = 0;
    context->clear_depth_value = 1.0f;
    context->draw_state_configured = 0;

    memset(&context->shader_code_mem, 0, sizeof(context->shader_code_mem));
    context->shaders_ready = 0;
    context->tile_alloc_stride = 32;

    return 0;
}

void v3d_context_free(V3DDevice* device, V3DContext* context)
{
    D(("vc4_context_free: enter\n"));

    v3d_frame_end(device, &context->frame);

    if (context->shaders_ready && context->shader_code_mem.handle)
    {
        v3d_mem_free(device, &context->shader_code_mem);
        context->shaders_ready = 0;
    }

    if (context->scratch_color_mem.handle)
    {
        v3d_mem_free(device, &context->scratch_color_mem);
        context->scratch_stride = 0;
    }

    if (context->mock_fb_mem.handle)
    {
        v3d_mem_free(device, &context->mock_fb_mem);
    }

    free_partial(device, context, 11);
}

int v3d_context_resize(V3DContext* context, V3DDevice* device, v3d_u16 width, v3d_u16 height)
{
    v3d_u32 zbuffer_size, tile_state_size, tile_alloc_size, tilesX, tilesY;

    if (width == context->width && height == context->height)
        return 0;

    v3d_frame_compute_pool_sizes(width, height, context->zbuffer_bits,
                                  &zbuffer_size, &tile_state_size,
                                  &tile_alloc_size, &tilesX, &tilesY);

    v3d_mem_free(device, &context->zbuffer_mem);
    v3d_mem_free(device, &context->tile_state_mem);
    v3d_mem_free(device, &context->tile_alloc_mem);

    if (v3d_mem_alloc(device, &context->zbuffer_mem, zbuffer_size + 4096) < 0) return -1;
    align_mem_4096(device, &context->zbuffer_mem);

    if (v3d_mem_alloc(device, &context->tile_state_mem, tile_state_size + 4096) < 0) return -2;
    align_mem_4096(device, &context->tile_state_mem);

    if (v3d_mem_alloc(device, &context->tile_alloc_mem, tile_alloc_size + 4096) < 0) return -3;
    align_mem_4096(device, &context->tile_alloc_mem);

    context->width = width;
    context->height = height;
    context->frame.tilesX = tilesX;
    context->frame.tilesY = tilesY;

    if (context->scratch_color_mem.handle)
    {
        v3d_mem_free(device, &context->scratch_color_mem);
        context->scratch_stride = 0;
    }

    return 0;
}

int v3d_backend_alloc_scratch_color(V3DDevice* device, V3DContext* context)
{
    v3d_u32 size;
    if (context->scratch_color_mem.handle)
        return 0;

    size = (v3d_u32)context->width * (v3d_u32)context->height * 4;
    context->scratch_stride = (v3d_u32)context->width * 4;

    if (v3d_mem_alloc(device, &context->scratch_color_mem, size + 4096) < 0)
        return -1;

    align_mem_4096(device, &context->scratch_color_mem);
    return 0;
}

void v3d_context_flush_for_dma(V3DDevice* device, V3DContext* context)
{
    struct ExecBase* const SysBase = device->sysbase;
    v3d_u8 slot = context->build_slot;
    v3d_static_buffer* bufs[4];

    bufs[0] = &context->binning_buf[slot];
    bufs[1] = &context->render_buf[slot];
    bufs[2] = &context->tile_list_buf[slot];
    bufs[3] = &context->state_buf[slot];

    for (int i = 0; i < 4; i++)
    {
        if (bufs[i]->used > 0)
        {
            ULONG len = (ULONG)bufs[i]->used;
            CachePreDMA(bufs[i]->start, &len, 0);
        }
    }
}
