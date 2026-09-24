/*
 * VideoCore IV (VC4) Texture implementation
 */

#include <exec/execbase.h>
#include <proto/exec.h>
#include <string.h>

#include "../include/v3d_texture.h"
#include "../include/v3d_context.h"
#include "../include/v3d_clbuf.h"
#include "../include/v3d_commands.h"
#include "vc4_hw.h"
#include "vc4_debug.h"

#ifndef V3D_ALIGN_UP
#define V3D_ALIGN_UP(val, align) (((val) + (align) - 1) & ~((align) - 1))
#endif

#define V3D_TEX_PARK_MAX 32
#define V3D_TEX_POOL_MAX 16

typedef struct {
    v3d_mem mem;
    int submitted;
} v3d_parked_tex;

static v3d_parked_tex s_park[V3D_TEX_PARK_MAX];
static int s_park_count = 0;

static v3d_mem s_pool[V3D_TEX_POOL_MAX];
static int s_pool_count = 0;

int g_mglv3d_texture_rename_fails = 0;

static int tex_pool_alloc(V3DDevice* device, v3d_mem* mem, v3d_u32 size)
{
    int i;
    for (i = 0; i < s_pool_count; i++)
    {
        if (s_pool[i].size >= size)
        {
            *mem = s_pool[i];
            s_pool[i] = s_pool[--s_pool_count];
            return 0;
        }
    }
    return v3d_mem_alloc(device, mem, size);
}

int v3d_texture_alloc(V3DDevice* device, V3DTexture* tex, v3d_u16 width, v3d_u16 height)
{
    if (!device || !tex) return -1;

    memset(tex, 0, sizeof(*tex));

    v3d_u32 padded_w = V3D_ALIGN_UP(width, 16);
    v3d_u32 padded_h = V3D_ALIGN_UP(height, 16);
    v3d_u32 size = padded_w * padded_h * 4 + 256;

    if (tex_pool_alloc(device, &tex->texture_mem, size) < 0)
        return -1;

    tex->width = width;
    tex->height = height;
    tex->format = V3D_SRCFMT_RGBA8;
    tex->min_filter = V3D_TEXFILTER_NEAREST;
    tex->mag_filter = V3D_TEXFILTER_NEAREST;
    tex->mip_filter_nearest = 1;
    tex->wrap_s = V3D_TEXWRAP_BORDER;
    tex->wrap_t = V3D_TEXWRAP_BORDER;
    tex->resident = 0;
    tex->dirty = 1;

    tex->num_levels = 1;
    tex->levels[0].offset = 0;
    tex->levels[0].stride = padded_w * 4;
    tex->levels[0].padded_w = (v3d_u16)padded_w;
    tex->levels[0].padded_h = (v3d_u16)padded_h;
    tex->levels[0].tiling_format = VC4_TILING_RASTER;
    tex->levels[0].ub_pad = 0;

    return 0;
}

void v3d_texture_free(V3DDevice* device, V3DTexture* tex)
{
    if (!tex) return;
    if (tex->texture_mem.hostptr)
    {
        v3d_mem_free(device, &tex->texture_mem);
    }
    memset(tex, 0, sizeof(*tex));
}

int v3d_texture_alloc_mipchain(V3DDevice* device, V3DTexture* tex, v3d_u8 want_levels)
{
    if (!tex || want_levels <= 1) return 0;
    if (want_levels > V3D_MAX_MIP_LEVELS) want_levels = V3D_MAX_MIP_LEVELS;
    if (tex->num_levels >= want_levels) return 0;

    /* Expand mip chain allocation */
    v3d_u32 total_size = 0;
    for (int i = 0; i < want_levels; i++)
    {
        v3d_u32 lw = tex->width >> i; if (!lw) lw = 1;
        v3d_u32 lh = tex->height >> i; if (!lh) lh = 1;
        lw = V3D_ALIGN_UP(lw, 16);
        lh = V3D_ALIGN_UP(lh, 16);
        tex->levels[i].offset = total_size;
        tex->levels[i].stride = lw * 4;
        tex->levels[i].padded_w = (v3d_u16)lw;
        tex->levels[i].padded_h = (v3d_u16)lh;
        tex->levels[i].tiling_format = VC4_TILING_RASTER;
        tex->levels[i].ub_pad = 0;
        total_size += lw * lh * 4;
    }

    v3d_mem new_mem;
    if (tex_pool_alloc(device, &new_mem, total_size + 256) < 0)
        return -1;

    /* Copy level 0 */
    if (tex->texture_mem.hostptr)
    {
        memcpy(new_mem.hostptr, tex->texture_mem.hostptr, tex->levels[0].stride * tex->levels[0].padded_h);
        v3d_texture_park_mem(device, &tex->texture_mem);
    }

    tex->texture_mem = new_mem;
    tex->num_levels = want_levels;
    return 0;
}

int v3d_texture_park_mem(V3DDevice* device, v3d_mem* mem)
{
    if (!mem || !mem->hostptr) return 0;

    if (s_park_count >= V3D_TEX_PARK_MAX)
    {
        v3d_mem_free(device, mem);
        return 0;
    }

    s_park[s_park_count].mem = *mem;
    s_park[s_park_count].submitted = 0;
    s_park_count++;

    memset(mem, 0, sizeof(*mem));
    return 0;
}

void v3d_texture_parked_mark_submitted(void)
{
    for (int i = 0; i < s_park_count; i++)
        s_park[i].submitted = 1;
}

void v3d_texture_drain_parked(V3DDevice* device)
{
    int i = 0;
    while (i < s_park_count)
    {
        if (!s_park[i].submitted)
        {
            i++;
            continue;
        }

        if (s_pool_count < V3D_TEX_POOL_MAX)
            s_pool[s_pool_count++] = s_park[i].mem;
        else
            v3d_mem_free(device, &s_park[i].mem);

        s_park[i] = s_park[--s_park_count];
    }
}

void v3d_texture_drain_parked_all(V3DDevice* device)
{
    int i;
    for (i = 0; i < s_park_count; i++)
        v3d_mem_free(device, &s_park[i].mem);
    s_park_count = 0;

    for (i = 0; i < s_pool_count; i++)
        v3d_mem_free(device, &s_pool[i]);
    s_pool_count = 0;
}

int v3d_texture_rename(V3DDevice* device, V3DTexture* tex, int copy_old)
{
    v3d_mem old_mem;
    v3d_u32 size;

    if (!tex || !tex->texture_mem.hostptr)
        return -1;

    size = tex->texture_mem.size;
    old_mem = tex->texture_mem;

    if (tex_pool_alloc(device, &tex->texture_mem, size) < 0)
    {
        tex->texture_mem = old_mem;
        g_mglv3d_texture_rename_fails++;
        return -1;
    }

    if (copy_old)
    {
        memcpy((char*)V3D_ALIGN_UP((v3d_uintptr)tex->texture_mem.hostptr, 256),
               (const char*)V3D_ALIGN_UP((v3d_uintptr)old_mem.hostptr, 256),
               (size_t)(size - 256));
    }

    v3d_texture_park_mem(device, &old_mem);
    return 0;
}

void v3d_texture_upload_rgba8_level(V3DDevice* device, V3DTexture* tex, v3d_u8 level,
                                     const void* src, v3d_u32 srcStride)
{
    if (!tex || !src || !tex->texture_mem.hostptr || level >= tex->num_levels)
        return;

    char* dst = (char*)V3D_ALIGN_UP((v3d_uintptr)tex->texture_mem.hostptr, 256) + tex->levels[level].offset;
    v3d_u32 w = tex->width >> level; if (!w) w = 1;
    v3d_u32 h = tex->height >> level; if (!h) h = 1;
    v3d_u32 dstStride = tex->levels[level].stride;

    const char* s = (const char*)src;
    for (v3d_u32 y = 0; y < h; y++)
    {
        memcpy(dst + y * dstStride, s + y * srcStride, w * 4);
    }

    if (level > tex->max_level_uploaded)
        tex->max_level_uploaded = level;

    tex->resident = 1;
    tex->dirty = 0;

    ULONG len = tex->texture_mem.size;
    CachePreDMA(tex->texture_mem.hostptr, &len, 0);
}

void v3d_texture_upload_rgba8(V3DDevice* device, V3DTexture* tex, const void* src, v3d_u32 srcStride)
{
    v3d_texture_upload_rgba8_level(device, tex, 0, src, srcStride);
}

void v3d_texture_upload_rgba8_subimage(V3DDevice* device, V3DTexture* tex, v3d_u8 level,
                                        const void* src, v3d_u32 srcStride,
                                        int xoffset, int yoffset, int width, int height)
{
    if (!tex || !src || !tex->texture_mem.hostptr || level >= tex->num_levels)
        return;

    char* dst = (char*)V3D_ALIGN_UP((v3d_uintptr)tex->texture_mem.hostptr, 256) + tex->levels[level].offset;
    v3d_u32 dstStride = tex->levels[level].stride;

    const char* s = (const char*)src;
    for (int y = 0; y < height; y++)
    {
        char* d = dst + (yoffset + y) * dstStride + (xoffset * 4);
        memcpy(d, s + y * srcStride, width * 4);
    }

    tex->resident = 1;
    tex->dirty = 0;

    ULONG len = tex->texture_mem.size;
    CachePreDMA(tex->texture_mem.hostptr, &len, 0);
}

void v3d_texture_convert_row(v3d_u32* dst, const v3d_u8* src, v3d_u32 count, v3d_u8 srcFormat)
{
    if (srcFormat == V3D_SRCFMT_RGBA8)
    {
        while (count--)
        {
            *dst++ = LE32(((v3d_u32)src[2] << 16) | ((v3d_u32)src[1] << 8) | ((v3d_u32)src[0] << 0) | ((v3d_u32)src[3] << 24));
            src += 4;
        }
    }
    else if (srcFormat == V3D_SRCFMT_ARGB8)
    {
        while (count--)
        {
            *dst++ = LE32(((v3d_u32)src[0] << 24) | ((v3d_u32)src[3] << 16) | ((v3d_u32)src[2] << 8) | ((v3d_u32)src[1] << 0));
            src += 4;
        }
    }
    else if (srcFormat == V3D_SRCFMT_RGB8)
    {
        while (count--)
        {
            *dst++ = LE32((0xffUL << 24) | ((v3d_u32)src[2] << 16) | ((v3d_u32)src[1] << 8) | ((v3d_u32)src[0] << 0));
            src += 3;
        }
    }
    else if (srcFormat == V3D_SRCFMT_ARGB4)
    {
        while (count--)
        {
            v3d_u16 val = *(const v3d_u16*)src;
            v3d_u32 a4 = (val >> 12) & 0xf, r4 = (val >> 8) & 0xf, g4 = (val >> 4) & 0xf, b4 = val & 0xf;
            v3d_u32 a8 = (a4 << 4) | a4, r8 = (r4 << 4) | r4, g8 = (g4 << 4) | g4, b8 = (b4 << 4) | b4;
            *dst++ = LE32((a8 << 24) | (b8 << 16) | (g8 << 8) | r8);
            src += 2;
        }
    }
    else if (srcFormat == V3D_SRCFMT_ARGB1555)
    {
        while (count--)
        {
            v3d_u16 val = *(const v3d_u16*)src;
            v3d_u32 a8 = (val & 0x8000) ? 255 : 0;
            v3d_u32 r5 = (val >> 10) & 0x1f, g5 = (val >> 5) & 0x1f, b5 = val & 0x1f;
            v3d_u32 r8 = (r5 << 3) | (r5 >> 2), g8 = (g5 << 3) | (g5 >> 2), b8 = (b5 << 3) | (b5 >> 2);
            *dst++ = LE32((a8 << 24) | (b8 << 16) | (g8 << 8) | r8);
            src += 2;
        }
    }
    else if (srcFormat == V3D_SRCFMT_RGB565)
    {
        while (count--)
        {
            v3d_u16 val = *(const v3d_u16*)src;
            v3d_u32 r5 = (val >> 11) & 0x1f, g6 = (val >> 5) & 0x3f, b5 = val & 0x1f;
            v3d_u32 r8 = (r5 << 3) | (r5 >> 2), g8 = (g6 << 2) | (g6 >> 4), b8 = (b5 << 3) | (b5 >> 2);
            *dst++ = LE32((0xffUL << 24) | (b8 << 16) | (g8 << 8) | r8);
            src += 2;
        }
    }
    else if (srcFormat == V3D_SRCFMT_LA8)
    {
        while (count--)
        {
            v3d_u8 l = src[0];
            v3d_u8 a = src[1];
            *dst++ = LE32(((v3d_u32)a << 24) | ((v3d_u32)l << 16) | ((v3d_u32)l << 8) | (v3d_u32)l);
            src += 2;
        }
    }
    else if (srcFormat == V3D_SRCFMT_L8)
    {
        while (count--)
        {
            v3d_u8 l = *src++;
            *dst++ = LE32((0xffUL << 24) | ((v3d_u32)l << 16) | ((v3d_u32)l << 8) | (v3d_u32)l);
        }
    }
    else if (srcFormat == V3D_SRCFMT_A8)
    {
        while (count--)
        {
            v3d_u8 a = *src++;
            *dst++ = LE32(((v3d_u32)a << 24) | 0x00ffffffUL);
        }
    }
}

void v3d_texture_emit_state(V3DDevice* device, V3DContext* context, V3DTexture* tex,
                             ULONG* outTextureShaderStateAddress, ULONG* outTextureSamplerStateAddress)
{
    char* data = (char*)V3D_ALIGN_UP((v3d_uintptr)tex->texture_mem.hostptr, 256);
    AlignBuffer(context, 16);

    v3d_texture_shader_state* ts = (v3d_texture_shader_state*)v3d_cl_claim_fast(
        device, &context->state_mem[context->build_slot], &context->state_buf[context->build_slot],
        sizeof(v3d_texture_shader_state), &context->frame);
    *outTextureShaderStateAddress = (ULONG)ts;
    memset(ts, 0, sizeof(*ts));

    ts->texture_base_pointer_rshift_6 = ((ULONG)data + tex->levels[0].offset) >> 6;
    ts->image_width_lo = tex->width & 0x3f;
    ts->image_width_hi = tex->width >> 6;
    ts->image_height = tex->height;
    ts->base_level = 0;
    ts->max_level = tex->max_level_uploaded;
    ts->texture_type = 4; /* RGBA8888 */

    ULONG* swivel = (ULONG*)ts;
    swivel[0] = LE32(swivel[0]);
    swivel[1] = LE32(swivel[1]);
    swivel[2] = LE32(swivel[2]);
    swivel[3] = LE32(swivel[3]);
    swivel[4] = LE32(swivel[4]);

    AlignBuffer(context, 16);
    v3d_sampler_state* ss = (v3d_sampler_state*)v3d_cl_claim_fast(
        device, &context->state_mem[context->build_slot], &context->state_buf[context->build_slot],
        sizeof(v3d_sampler_state), &context->frame);
    *outTextureSamplerStateAddress = (ULONG)ss;
    memset(ss, 0, sizeof(*ss));

    UBYTE minFilt = (tex->min_filter == V3D_TEXFILTER_LINEAR || tex->min_filter == V3D_TEXFILTER_LINEAR_MIPMAP) ? 0 : 1;
    UBYTE magFilt = (tex->mag_filter == V3D_TEXFILTER_LINEAR || tex->mag_filter == V3D_TEXFILTER_LINEAR_MIPMAP) ? 0 : 1;
    ss->min_filter_nearest = minFilt;
    ss->mag_filter_nearest = magFilt;
    ss->mip_filter_nearest = tex->mip_filter_nearest ? 1 : 0;
    ss->wrap_s = (tex->wrap_s == V3D_TEXWRAP_REPEAT) ? 0 : (tex->wrap_s == V3D_TEXWRAP_CLAMP ? 1 : 2);
    ss->wrap_t = (tex->wrap_t == V3D_TEXWRAP_REPEAT) ? 0 : (tex->wrap_t == V3D_TEXWRAP_CLAMP ? 1 : 2);

    UBYTE* swivel24 = (UBYTE*)ss;
    UBYTE temp = swivel24[3];
    swivel24[3] = swivel24[1];
    swivel24[1] = temp;

    UWORD* swivel16 = (UWORD*)ss;
    swivel16[3] = LE16(swivel16[3]);
}
