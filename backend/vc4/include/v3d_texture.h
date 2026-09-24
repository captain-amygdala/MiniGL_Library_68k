/*
 * VideoCore IV (VC4) Texture management (T-Format and Linear)
 */

#ifndef VC4_TEXTURE_H
#define VC4_TEXTURE_H

#include "v3d_types.h"
#include "v3d_device.h"

#ifndef V3D_CONTEXT_TYPEDEF_DEFINED
#define V3D_CONTEXT_TYPEDEF_DEFINED
typedef struct V3DContext V3DContext;
#endif

enum {
    V3D_TEXFILTER_NEAREST = 0,
    V3D_TEXFILTER_LINEAR,
    V3D_TEXFILTER_NEAREST_MIPMAP,
    V3D_TEXFILTER_LINEAR_MIPMAP
};

enum {
    V3D_TEXWRAP_REPEAT = 0,
    V3D_TEXWRAP_CLAMP,
    V3D_TEXWRAP_BORDER
};

enum {
    V3D_TEXENV_MODULATE = 0,
    V3D_TEXENV_DECAL,
    V3D_TEXENV_REPLACE
};

enum {
    V3D_SRCFMT_RGBA8 = 0,
    V3D_SRCFMT_ARGB8,
    V3D_SRCFMT_RGB8,
    V3D_SRCFMT_ARGB4,
    V3D_SRCFMT_ARGB1555,
    V3D_SRCFMT_RGB565,
    V3D_SRCFMT_LA8,
    V3D_SRCFMT_L8,
    V3D_SRCFMT_A8
};

enum {
    VC4_TILING_RASTER = 0,
    VC4_TILING_TFORMAT,
    VC4_TILING_LT
};

#define V3D_MAX_MIP_LEVELS 13

typedef struct V3DTexture {
    struct V3DTexture* next;
    v3d_mem texture_mem;
    v3d_u16 width, height;
    v3d_u8  format;
    v3d_u8  mipmap_count;
    v3d_u8  min_filter, mag_filter;
    v3d_u8  mip_filter_nearest;
    v3d_u8  wrap_s, wrap_t;
    v3d_u8  border_r, border_g, border_b, border_a;
    v3d_u8  texenv_mode;
    v3d_u8  env_r, env_g, env_b, env_a;
    v3d_u8  resident;
    v3d_u8  dirty;
    v3d_u8  tiling_format;
    v3d_u8  ub_pad;
    v3d_u8  num_levels;
    v3d_u8  max_level_uploaded;
    struct {
        v3d_u32 offset;
        v3d_u32 stride;
        v3d_u16 padded_w, padded_h;
        v3d_u8  tiling_format;
        v3d_u8  ub_pad;
    } levels[V3D_MAX_MIP_LEVELS];
    v3d_u8  no_alpha;
    v3d_u32 last_draw_frame;
} V3DTexture;

int  v3d_texture_alloc(V3DDevice* device, V3DTexture* tex, v3d_u16 width, v3d_u16 height);
void v3d_texture_free(V3DDevice* device, V3DTexture* tex);
int  v3d_texture_alloc_mipchain(V3DDevice* device, V3DTexture* tex, v3d_u8 want_levels);
int  v3d_texture_park_mem(V3DDevice* device, v3d_mem* mem);
void v3d_texture_parked_mark_submitted(void);
void v3d_texture_drain_parked(V3DDevice* device);
void v3d_texture_drain_parked_all(V3DDevice* device);
int  v3d_texture_rename(V3DDevice* device, V3DTexture* tex, int copy_old);

void v3d_texture_upload_rgba8(V3DDevice* device, V3DTexture* tex, const void* src, v3d_u32 srcStride);
void v3d_texture_upload_rgba8_level(V3DDevice* device, V3DTexture* tex, v3d_u8 level,
                                     const void* src, v3d_u32 srcStride);
void v3d_texture_upload_rgba8_subimage(V3DDevice* device, V3DTexture* tex, v3d_u8 level,
                                        const void* src, v3d_u32 srcStride,
                                        int xoffset, int yoffset, int width, int height);

void v3d_texture_convert_row(v3d_u32* dst, const v3d_u8* src, v3d_u32 count, v3d_u8 srcFormat);
void v3d_texture_emit_state(V3DDevice* device, V3DContext* context, V3DTexture* tex,
                             ULONG* outTextureShaderStateAddress, ULONG* outTextureSamplerStateAddress);

#endif /* VC4_TEXTURE_H */
