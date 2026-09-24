/*
 * VideoCore IV (VC4) Context definition
 */

#ifndef VC4_CONTEXT_H
#define VC4_CONTEXT_H

#include "v3d_types.h"
#include "v3d_texture.h"
#include "v3d_frame.h"
#include "v3d_device.h"

#define V3D_MAX_TEXUNIT 2

#define V3D_INITIAL_CL_BUFFER_SIZE 4194304
#define V3D_INITIAL_RENDER_BUFFER_SIZE 65536
#define V3D_INITIAL_TILE_LIST_BUFFER_SIZE 32768

enum {
    V3D_FOG_LINEAR = 0,
    V3D_FOG_EXP,
    V3D_FOG_EXP2
};

enum {
    V3D_ALPHAFUNC_NEVER = 0,
    V3D_ALPHAFUNC_LESS,
    V3D_ALPHAFUNC_EQUAL,
    V3D_ALPHAFUNC_LEQUAL,
    V3D_ALPHAFUNC_GREATER,
    V3D_ALPHAFUNC_NOTEQUAL,
    V3D_ALPHAFUNC_GEQUAL,
    V3D_ALPHAFUNC_ALWAYS
};

enum {
    V3D_Z_NEVER = 0,
    V3D_Z_LESS,
    V3D_Z_EQUAL,
    V3D_Z_LEQUAL,
    V3D_Z_GREATER,
    V3D_Z_NOTEQUAL,
    V3D_Z_GEQUAL,
    V3D_Z_ALWAYS
};

enum {
    V3D_EZ_UNDECIDED = 0,
    V3D_EZ_GT_GE,
    V3D_EZ_LT_LE,
    V3D_EZ_DISABLED
};

#ifndef V3D_CONTEXT_TYPEDEF_DEFINED
#define V3D_CONTEXT_TYPEDEF_DEFINED
typedef struct V3DContext V3DContext;
#endif

struct V3DContext {
    void*   framebuffer;
    v3d_u16 width, height;
    v3d_i32 y_offset;
    int     zbuffer_bits;

    v3d_mem zbuffer_mem;
    v3d_mem tile_state_mem;
    v3d_mem tile_alloc_mem;

    v3d_mem binning_mem[2], render_mem[2], tile_list_mem[2], state_mem[2];
    v3d_static_buffer binning_buf[2];
    v3d_static_buffer render_buf[2];
    v3d_static_buffer tile_list_buf[2];
    v3d_static_buffer state_buf[2];
    v3d_static_buffer* current_buf;
    v3d_u8 build_slot;

    v3d_u8 render_lastframe;
    v3d_u8 render_pending;

    V3DFrame frame;

    v3d_u32 fixed_color;
    v3d_u32 zmode;
    v3d_u32 blend_srcmode, blend_dstmode;
    v3d_u32 blend_alpha_srcmode, blend_alpha_dstmode;
    v3d_u32 blend_color_equation, blend_alpha_equation;
    v3d_u8  blend_nonadd_warned;
    v3d_u8  alpha_test_enable;
    v3d_u8  alpha_func;
    float   alpha_ref;
    v3d_u8  color_mask_r, color_mask_g, color_mask_b, color_mask_a;

    v3d_u16 scissor_x, scissor_y, scissor_w, scissor_h;
    v3d_u8  scissor_enable;

    v3d_u8  fog_enable;
    v3d_u8  fog_mode;
    float   fog_start, fog_end, fog_density;
    v3d_u8  fog_r, fog_g, fog_b, fog_a;

    V3DTexture* bound_texture[V3D_MAX_TEXUNIT];
    V3DTexture* texture_list;

    v3d_u8 frame_active;
    v3d_u8 pending_clear_color;
    v3d_u8 pending_clear_depth;
    v3d_u8 frame_corrupted;

    v3d_u8 ez_state;
    v3d_u8 first_ez_state;
    v3d_u8 depth_ez_dir;
    v3d_u8 depth_persist_seen;

    v3d_u8 force_new_pass;
    v3d_u8 doing_intermediate_pass;
    v3d_u8 has_scratch_color;
    v3d_mem scratch_color_mem;
    v3d_u32 scratch_stride;

    v3d_u32 clear_color_value;
    float   clear_depth_value;
    v3d_u8 draw_state_configured;

    v3d_mem shader_code_mem;
    v3d_u8  shaders_ready;
    v3d_mem mock_fb_mem;
};

int  v3d_context_init(V3DContext* context, V3DDevice* device, v3d_u16 width, v3d_u16 height);
void v3d_context_free(V3DDevice* device, V3DContext* context);
int  v3d_context_resize(V3DContext* context, V3DDevice* device, v3d_u16 width, v3d_u16 height);
int  v3d_backend_alloc_scratch_color(V3DDevice* device, V3DContext* context);
void v3d_context_flush_for_dma(V3DDevice* device, V3DContext* context);

#endif /* VC4_CONTEXT_H */
