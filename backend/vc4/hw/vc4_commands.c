/*
 * VideoCore IV (VC4) Command packet writers
 */

#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include <exec/execbase.h>
#include <proto/exec.h>

#include "vc4_debug.h"
#include "vc4_regs.h"
#include "vc4_hw.h"
#include "../include/v3d_context.h"
#include "../include/v3d_commands.h"
#include "../include/v3d_clbuf.h"

float swap_float32(float val)
{
    ULONG u;
    float res;
    memcpy(&u, &val, sizeof(u));
    u = LE32(u);
    memcpy(&res, &u, sizeof(res));
    return res;
}

void swap_float32_into(void *dst, float val)
{
    ULONG u;
    memcpy(&u, &val, sizeof(u));
    *(ULONG *)dst = LE32(u);
}

ULONG float_to_ulong(float f)
{
    return *(ULONG*)&f;
}

void AlignBuffer(V3DContext* context, ULONG alignment)
{
    v3d_static_buffer* buffer = context->current_buf;
    if (buffer) {
        buffer->used = (buffer->used + (alignment - 1)) & ~(alignment - 1);
    }
}

void SetBuffer(V3DContext* context, v3d_static_buffer* buffer)
{
    context->current_buf = buffer;
}

ULONG CurrentBufferAddress(V3DContext* context)
{
    v3d_static_buffer* buffer = context->current_buf;
    if (!buffer) return 0;
    return ((ULONG)buffer->start + buffer->used);
}

void DoCommand(V3DContext* context, UBYTE command)
{
    /* On VC4, END_OF_RENDERING is handled by STORE_MULTI_SAMPLE_RESOLVED_TILE_COLOR_BUFFER_AND_EOF (opcode 25).
     * Do not emit trailing NOPs after opcode 25 so CT1EA matches the EOF halt point exactly. */
    if (command == v3d_OP_END_OF_RENDERING)
        return;

    if (command == v3d_OP_START_TILE_BINNING)
    {
        v3d_static_buffer* buffer = context->current_buf;
        UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 3);
        if (p) {
            p[0] = v3d_OP_START_TILE_BINNING; /* 6 */
            /* Mesa vc4_draw.c: PRIMITIVE_LIST_FORMAT is emitted once right after START_TILE_BINNING
             * to initialize the default compressed primitive format to 16-bit indices, triangles. */
            p[1] = v3d_OP_PRIM_LIST_FORMAT;   /* 56 */
            p[2] = 0x12; /* 16-bit index (0x10) | Triangles (0x02) */
        }
        return;
    }

    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 1);
    if (p) {
        *p = command;
    }
}

void TileBinningModeCfg(V3DContext* context, UBYTE taibz, UBYTE tabz, UBYTE targets, UBYTE bpp, BOOL msaa, BOOL db, UWORD width, UWORD height)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 16);
    if (!p) return;

    UWORD tile_w = (width + 63) / 64;
    UWORD tile_h = (height + 63) / 64;
    ULONG tile_alloc_addr = (ULONG)context->tile_alloc_mem.busaddr;
    ULONG tile_alloc_size = context->tile_alloc_mem.size;
    ULONG tile_state_addr = (ULONG)context->tile_state_mem.busaddr;

    p[0] = v3d_OP_TILE_BINNING_MODE_CFG; /* 112 */
    *(ULONG*)(p + 1) = LE32(tile_alloc_addr);
    *(ULONG*)(p + 5) = LE32(tile_alloc_size);
    *(ULONG*)(p + 9) = LE32(tile_state_addr);
    p[13] = (UBYTE)tile_w;
    p[14] = (UBYTE)tile_h;

    /* Flags:
     * bit 0 (112): msaa
     * bit 1 (113): 64-bit color depth
     * bit 2 (114): auto-init tile state data array (1)
     * bit 3..4 (115..116): initial block size (0=32 bytes, required by VC4 TSDA auto-init)
     * bit 5..6 (117..118): block size (0=32, 1=64, 2=128, 3=256)
     * bit 7 (119): double buffer
     */
    UBYTE flags = (msaa ? 1 : 0) | (1 << 2) | (0 << 3) | ((tabz & 3) << 5) | (db ? (1 << 7) : 0);
    p[15] = flags;
    context->tile_alloc_stride = 32;
}

void OcclusionQueryCounter(V3DContext* context, ULONG address)
{
    /* No-op on VC4 */
}

void ClipWindow(V3DContext* context, UWORD lpc, UWORD bpc, UWORD width, UWORD height)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 9);
    if (!p) return;

    p[0] = v3d_OP_CLIPWINDOW; /* 102 */
    *(UWORD*)(p + 1) = LE16(lpc);
    *(UWORD*)(p + 3) = LE16(bpc);
    *(UWORD*)(p + 5) = LE16(width);
    *(UWORD*)(p + 7) = LE16(height);
}

void CfgBits(V3DContext* context, UBYTE effp, UBYTE erfp, UBYTE cp, UBYTE edo, UBYTE lr, UBYTE rom, UBYTE d3dwftm,
             UBYTE dtf, UBYTE zue, UBYTE eze, UBYTE ezue, UBYTE se, UBYTE be, UBYTE d3dpfm, UBYTE d3dpv)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 4);
    if (!p) return;

    p[0] = v3d_OP_CFG_BITS; /* 96 */
    ULONG bits = (effp ? 1 : 0) |
                 (erfp ? (1 << 1) : 0) |
                 (cp ? (1 << 2) : 0) |
                 (edo ? (1 << 3) : 0) |
                 ((rom & 3) << 6) |
                 ((dtf & 7) << 12) |
                 (zue ? (1 << 15) : 0) |
                 (eze ? (1 << 16) : 0) |
                 (ezue ? (1 << 17) : 0);
    p[1] = (UBYTE)(bits & 0xFF);
    p[2] = (UBYTE)((bits >> 8) & 0xFF);
    p[3] = (UBYTE)((bits >> 16) & 0xFF);
}

void PointSize(V3DContext* context, float size)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 5);
    if (!p) return;
    p[0] = v3d_OP_POINT_SIZE; /* 98 */
    swap_float32_into(p + 1, size);
}

void LineWidth(V3DContext* context, float width)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 5);
    if (!p) return;
    p[0] = v3d_OP_LINE_WIDTH; /* 99 */
    swap_float32_into(p + 1, width);
}

void DepthOffset(V3DContext* context, float factor, float units)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 5);
    if (!p) return;
    p[0] = v3d_OP_DEPTH_OFFSET; /* 101 */
    *(UWORD*)(p + 1) = LE16(v3d_float_to_f187(factor));
    *(UWORD*)(p + 3) = LE16(v3d_float_to_f187(units));
}

void ClipperXYScaling(V3DContext* context, UWORD width, UWORD height)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 9);
    if (!p) return;
    p[0] = v3d_OP_CLIPPER_XY_SCALING; /* 105 */
    /* width/height passed from draw.c are 2.0f * context->sx/sy, so (width/2) is half-width/height.
     * VC4 packet 105 takes half-width/half-height in 1/16th of a pixel as float32.
     * Y is flipped (-16.0f) to match the inverted Y direction in screen space. */
    float half_w_16th = (float)(width / 2) * 16.0f;
    float half_h_16th = (float)(height / 2) * -16.0f;
    swap_float32_into(p + 1, half_w_16th);
    swap_float32_into(p + 5, half_h_16th);
}

void ClipperZScaleAndOffset(V3DContext* context, float vpzs, float vpzo)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 9);
    if (!p) return;
    p[0] = v3d_OP_CLIPPER_Z_SCALE_AND_OFFSET; /* 106 */
    swap_float32_into(p + 1, vpzs);
    swap_float32_into(p + 5, vpzo);
}

void ClipperZMinMaxClippingPlanes(V3DContext* context, float minzw, float maxzw)
{
    (void)context;
    (void)minzw;
    (void)maxzw;
    /* No-op on VC4: Mesa vc4 driver and rpi-hal omit opcode 104 entirely.
     * Hardware clipper uses CLIPPER_Z_SCALE_AND_OFFSET (opcode 106). */
}

void ViewportOffset(V3DContext* context, UWORD width, UWORD coarse_x, UWORD height, UWORD coarse_y)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 5);
    if (!p) return;
    p[0] = v3d_OP_VIEWPORT_OFFSET; /* 103 */
    /* width and height passed from draw.c are 2.0f * context->ax/ay, so width/2 is centre X/Y.
     * Viewport Centre coordinates are in s12.4 fixed point: (width/2) * 16 = width * 8. */
    int16_t cx = (int16_t)((width / 2) * 16);
    int16_t cy = (int16_t)((height / 2) * 16);
    *(UWORD*)(p + 1) = LE16((v3d_u16)cx);
    *(UWORD*)(p + 3) = LE16((v3d_u16)cy);
}

void ColorWriteMasks(V3DContext* context, ULONG mask)
{
    /* No-op on VC4 */
}

void BlendConstantColor(V3DContext* context, UWORD alpha, UWORD blue, UWORD green, UWORD red)
{
    /* No-op on VC4 */
}

void BlendEnables(V3DContext* context, UBYTE mask)
{
    /* No-op on VC4 */
}

void BlendCfg(V3DContext* context, UBYTE renderTargetMask, UBYTE colorDstFactor, UBYTE colorSrcFactor,
              UBYTE colorMode, UBYTE alphaDstFactor, UBYTE alphaSrcFactor, UBYTE alphaMode)
{
    /* No-op on VC4 */
}

void TransformFeedbackSpecs(V3DContext* context, UBYTE enable, UBYTE no16b)
{
    /* No-op on VC4 */
}

void SampleState(V3DContext* context, UWORD coverage, UBYTE mask)
{
    /* No-op on VC4 */
}

void VCMCacheSize(V3DContext* context, UBYTE cbinning, UBYTE crendering)
{
    /* No-op on VC4 */
}

void NumberOfLayers(V3DContext* context, UBYTE layers)
{
    /* No-op on VC4 */
}

void glShaderState(V3DContext* context, ULONG staterecordAddress, ULONG attr_count)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 5);
    if (!p) return;
    p[0] = v3d_OP_GL_SHADER_STATE; /* 64 */
    ULONG val = (staterecordAddress & ~0xF) | (attr_count & 0x7);
    *(ULONG*)(p + 1) = LE32(val);
}

void VertexArrayPrims(V3DContext* context, UBYTE mode, ULONG length, ULONG index)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 10);
    if (!p) return;
    p[0] = v3d_OP_VERTEX_ARRAY_PRIMS; /* 33 */
    p[1] = mode;
    *(ULONG*)(p + 2) = LE32(length);
    *(ULONG*)(p + 6) = LE32(index);
}

void VertexArrayInstancedPrims(V3DContext* context, UBYTE mode, ULONG length, ULONG instances, ULONG index)
{
    VertexArrayPrims(context, mode, length, index);
}

void IndexedPrimList(V3DContext* context, UBYTE mode, UBYTE indexType, ULONG length, BOOL enablePrimitiveRestarts, ULONG indexOffset)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 14);
    if (!p) return;
    p[0] = v3d_OP_INDEXED_PRIM_LIST; /* 32 */
    p[1] = (UBYTE)((mode & 0xF) | ((indexType & 0xF) << 4));
    *(ULONG*)(p + 2) = LE32(length);
    *(ULONG*)(p + 6) = LE32(indexOffset);
    *(ULONG*)(p + 10) = LE32(0xFFFF);
}

void IndexBufferSetup(V3DContext* context, ULONG address, ULONG size)
{
    /* No-op on VC4 */
}

void PrimListFormat(V3DContext* context, UBYTE type, UBYTE tsof)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 2);
    if (!p) return;
    p[0] = v3d_OP_PRIM_LIST_FORMAT; /* 56 */
    p[1] = (UBYTE)((type & 0xF) | (1 << 4)); /* 16-bit index */
}

void BranchToImplicitTileList(V3DContext* context, UBYTE tlsn)
{
    /* No-op on VC4 */
}

void ClearTileBuffers(V3DContext* context, UBYTE clearzstencil, UBYTE clearallrender)
{
    /* No-op on VC4 */
}

static void emit_tile_rendering_mode_cfg(V3DContext* context)
{
    if (!context->render_mode_cfg_pending) return;
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 11);
    if (!p) return;

    p[0] = v3d_OP_TILE_RENDERING_MODE_CFG; /* 113 */
    ULONG mem_addr = (ULONG)context->framebuffer;
    UWORD stride_pixels = context->fb_stride ? (UWORD)(context->fb_stride / 4) : context->render_mode_width;
    *(ULONG*)(p + 1) = LE32(mem_addr);
    *(UWORD*)(p + 5) = LE16(stride_pixels);
    *(UWORD*)(p + 7) = LE16(context->render_mode_height);
    p[9] = context->render_mode_flags9;
    p[10] = context->render_mode_flags10;
    context->render_mode_cfg_pending = 0;
    D(("emit_tile_rendering_mode_cfg: 113 emitted, fb=0x%08lx stride_px=%lu %lux%lu flags=0x%02lx,0x%02lx\n",
        mem_addr, (ULONG)stride_pixels, (ULONG)context->render_mode_width, (ULONG)context->render_mode_height,
        (ULONG)context->render_mode_flags9, (ULONG)context->render_mode_flags10));
}

void StoreTileBufferGeneral(V3DContext* context, UBYTE bts, UBYTE mf, UBYTE flipy, ULONG ditm,
                            ULONG decm, ULONG oif, ULONG cbbs, ULONG cr, ULONG rbswap, ULONG strideorub,
                            ULONG height, ULONG address)
{
    if (strideorub > 0) {
        context->fb_stride = strideorub;
    }

    if (bts == v3d_NONE) {
        if (context->clear_colors_emitted == 2) {
            /* Dummy store in None mode to latch clear colors */
            v3d_static_buffer* buffer = context->current_buf;
            UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 7);
            if (p) {
                p[0] = v3d_OP_STORE_TILE_BUFFER_GENERAL; /* 28 */
                p[1] = 0;
                p[2] = 0;
                *(ULONG*)(p + 3) = 0;
            }
            context->clear_colors_emitted = 3;
            return;
        } else if (context->clear_colors_emitted >= 3) {
            /* Redundant second dummy store: skip */
            return;
        }
    }

    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 7);
    if (!p) return;
    p[0] = v3d_OP_STORE_TILE_BUFFER_GENERAL; /* 28 */
    p[1] = (UBYTE)((bts & 7) | ((mf & 3) << 4) | ((decm & 3) << 6));
    p[2] = (UBYTE)(oif & 3);
    *(ULONG*)(p + 3) = LE32(address & ~15);
}

void LoadTileBufferGeneral(V3DContext* context, UBYTE btl, UBYTE mf, UBYTE flipy,
                            ULONG decm, ULONG iif, ULONG fa1, ULONG cr, ULONG rbswap,
                            ULONG strideorub, ULONG height, ULONG address)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 7);
    if (!p) return;
    p[0] = v3d_OP_LOAD_TILE_BUFFER_GENERAL; /* 29 */
    p[1] = (UBYTE)((btl & 7) | ((mf & 3) << 4) | ((decm & 3) << 6));
    p[2] = (UBYTE)(iif & 3);
    *(ULONG*)(p + 3) = LE32(address);
}

void TileRenderingModeCFGCommon(V3DContext* context, UBYTE targets, UWORD width, UWORD height,
                                UWORD maxbpp, UWORD msm, UWORD dbinmsm, UWORD eztaud, UWORD ezd,
                                UWORD idt, UWORD edsc)
{
    context->render_mode_width = width;
    context->render_mode_height = height;

    /* Byte 9: format flags */
    UBYTE color_fmt = (maxbpp == 16) ? 0 : 1; /* 1 = RGBA8888, 0 = BGR565 */
    UBYTE mem_fmt = 0; /* 0 = Raster */
    context->render_mode_flags9 = (UBYTE)((color_fmt << 2) | (mem_fmt << 6));

    /* Byte 10: early-z and double buffer */
    UBYTE ez_flags = (eztaud ? (1 << 2) : 0) | (ezd ? (1 << 3) : 0) | (dbinmsm ? (1 << 4) : 0);
    context->render_mode_flags10 = ez_flags;
    context->render_mode_cfg_pending = 1;
    context->clear_colors_emitted = 0;
}

void TileCoordinates(V3DContext* context, ULONG column, ULONG row)
{
    /* If clear colors was emitted, the dummy tile coordinates (0, 0) must precede the dummy store */
    if (context->clear_colors_emitted == 1) {
        context->clear_colors_emitted = 2;
    } else if (context->clear_colors_emitted == 2 || context->clear_colors_emitted == 3) {
        /* Redundant second dummy tile coordinate before rendering mode cfg: skip */
        return;
    }

    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 3);
    if (!p) return;
    p[0] = v3d_OP_TILE_COORDINATES; /* 115 */
    p[1] = (UBYTE)column;
    p[2] = (UBYTE)row;
}

void StartAddressOfGenericTileList(V3DContext* context, ULONG start, ULONG end)
{
    /* If packet 113 was not emitted yet, emit it now before any tiles */
    if (context->render_mode_cfg_pending) {
        emit_tile_rendering_mode_cfg(context);
    }

    /* Real tiles start here -- allow TileCoordinates to emit normally */
    context->clear_colors_emitted = 4;

    UWORD tile_w = (context->width + 63) / 64;
    UWORD tile_h = (context->height + 63) / 64;
    ULONG tile_alloc_base = (ULONG)context->tile_alloc_mem.busaddr;
    UWORD stride = context->tile_alloc_stride ? context->tile_alloc_stride : 32;

    D(("StartAddressOfGenericTileList: emitting %lu (%lux%lu) tiles, stride=%lu tile_alloc_base=0x%08lx\n",
        (ULONG)(tile_w * tile_h), (ULONG)tile_w, (ULONG)tile_h, (ULONG)stride, tile_alloc_base));

    for (UWORD y = 0; y < tile_h; y++)
    {
        for (UWORD x = 0; x < tile_w; x++)
        {
            TileCoordinates(context, x, y);

            /* Branch to sublist (opcode 17) */
            v3d_static_buffer* buffer = context->current_buf;
            UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 5);
            if (p) {
                p[0] = v3d_OP_BRANCH_TO_SUB_LIST; /* 17 */
                ULONG tile_offset = (ULONG)(y * tile_w + x) * stride;
                *(ULONG*)(p + 1) = LE32(tile_alloc_base + tile_offset);
            }

            /* Store tile color buffer */
            if (x == tile_w - 1 && y == tile_h - 1)
            {
                DoCommand(context, v3d_OP_STORE_MULTI_SAMPLE_RESOLVED_TILE_COLOR_BUFFER_AND_EOF); /* 25 */
            }
            else
            {
                DoCommand(context, v3d_OP_STORE_MULTI_SAMPLE_RESOLVED_TILE_COLOR_BUFFER); /* 24 */
            }
        }
    }
}

void Supertile_Coordinates(V3DContext* context, ULONG colnum, ULONG rownum)
{
    /* No-op on VC4 */
}

void Multicore_Rendering_Tile_List_Set_Base(V3DContext* context, ULONG address, ULONG tlsn)
{
    /* No-op on VC4 */
}

void TileListInitialBlockSize(V3DContext* context, UBYTE size, UBYTE useauto)
{
    /* No-op on VC4 */
}

void TileRenderingModeCFGZSClearValues(V3DContext* context, UBYTE scv, float zcv)
{
    /* Stored in context for clear colors packet */
    context->clear_depth_value = zcv;
}

void MulticoreRenderingSupertileCFG(V3DContext* context, UBYTE supertile_w, UBYTE supertile_h,
                                    UBYTE frame_w_supertile, UBYTE frame_h_supertile, UBYTE nobtl,
                                    UBYTE sro, UBYTE me, ULONG tilesY, ULONG tilesX)
{
    /* No-op on VC4 */
}

void TileRenderingModeCFGClearColorsPart1(V3DContext* context, UBYTE target, ULONG color)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 14);
    if (!p) return;

    p[0] = v3d_OP_TILE_RENDERING_CLEAR_COLORS; /* 114 */
    *(ULONG*)(p + 1) = LE32(color);
    *(ULONG*)(p + 5) = LE32(color);

    ULONG clear_zs = (ULONG)(context->clear_depth_value * 16777215.0f) & 0xFFFFFF;
    p[9]  = (UBYTE)(clear_zs & 0xFF);
    p[10] = (UBYTE)((clear_zs >> 8) & 0xFF);
    p[11] = (UBYTE)((clear_zs >> 16) & 0xFF);
    p[12] = 0xFF; /* Clear VG mask */
    p[13] = 0x00; /* Clear stencil */

    context->clear_colors_emitted = 1;
    D(("TileRenderingModeCFGClearColorsPart1: 114 emitted, color=0x%08lx zs=0x%06lx\n",
        color, clear_zs));

    /* Emit 113 immediately after 114 (per rpi-GLES and VC4 spec), before tile coordinates and dummy store */
    emit_tile_rendering_mode_cfg(context);
}

void TileRenderingModeCFGColor(V3DContext* context, UBYTE bpp, UBYTE type, UBYTE clamp)
{
    /* No-op on VC4 */
}

void glShaderStateAttributeRecord(V3DContext* context, APTR address, UBYTE raiu, UBYTE nit,
                                  UBYTE sit, UBYTE vsize, UBYTE vtype, UBYTE novrbvs, UBYTE novrbcs,
                                  UWORD divisor, ULONG stride, ULONG maxindex)
{
    v3d_static_buffer* buffer = context->current_buf;
    v3d_gl_shader_state_attribute_record* attr =
        (v3d_gl_shader_state_attribute_record*)v3d_buffer_claim_memory(buffer, sizeof(v3d_gl_shader_state_attribute_record));
    if (!attr) return;

    memset(attr, 0, sizeof(*attr));
    attr->address = LE32((ULONG)address);
    attr->number_of_bytes_minus_1 = (UBYTE)(stride ? (stride - 1) : 15);
    attr->stride = (UBYTE)stride;
    attr->vertex_shader_vpm_offset = novrbvs;
    attr->coordinate_shader_vpm_offset = novrbcs;
}

void SetInstanceid(V3DContext* context, ULONG instanceid)
{
    /* No-op on VC4 */
}

static inline float read_le_float(const void* p)
{
    ULONG u;
    float f;
    memcpy(&u, p, sizeof(u));
    u = LE32(u);
    memcpy(&f, &u, sizeof(f));
    return f;
}

void v3d_emit_primitive(V3DDevice* device, V3DContext* context, const v3d_primitive_args* args)
{
    int count = args->count;
    if (count <= 0) return;

    v3d_static_buffer* sb = &context->state_buf[context->build_slot];
    v3d_mem* sm = &context->state_mem[context->build_slot];

    /* 1. Claim memory for shaded vertices (16-byte aligned) */
    sb->used = (sb->used + 15) & ~15;
    VC4NVShadedVertex* out_verts = (VC4NVShadedVertex*)v3d_cl_claim_fast(
        device, sm, sb, count * sizeof(VC4NVShadedVertex), &context->frame);
    if (!out_verts) return;

    /* Matrix coefficients */
    float M_00 = 1.0f, M_10 = 0.0f, M_20 = 0.0f, M_30 = 0.0f;
    float M_01 = 0.0f, M_11 = 1.0f, M_21 = 0.0f, M_31 = 0.0f;
    float M_02 = 0.0f, M_12 = 0.0f, M_22 = 1.0f, M_32 = 0.0f;
    float M_03 = 0.0f, M_13 = 0.0f, M_23 = 0.0f, M_33 = 1.0f;

    if (!args->use_clip_space && args->matrix)
    {
        M_00 = args->matrix[0];  M_01 = args->matrix[1];  M_02 = args->matrix[2];  M_03 = args->matrix[3];
        M_10 = args->matrix[4];  M_11 = args->matrix[5];  M_12 = args->matrix[6];  M_13 = args->matrix[7];
        M_20 = args->matrix[8];  M_21 = args->matrix[9];  M_22 = args->matrix[10]; M_23 = args->matrix[11];
        M_30 = args->matrix[12]; M_31 = args->matrix[13]; M_32 = args->matrix[14]; M_33 = args->matrix[15];
    }

    int i;
    for (i = 0; i < count; i++)
    {
        float x = read_le_float(&args->posbuf[i * args->pos_stride_floats + 0]);
        float y = read_le_float(&args->posbuf[i * args->pos_stride_floats + 1]);
        float z = read_le_float(&args->posbuf[i * args->pos_stride_floats + 2]);
        float w = (args->pos_stride_floats >= 4) ? read_le_float(&args->posbuf[i * args->pos_stride_floats + 3]) : 1.0f;

        float xc, yc, zc, wc;
        if (args->use_clip_space)
        {
            xc = x; yc = y; zc = z; wc = w;
        }
        else
        {
            xc = x * M_00 + y * M_10 + z * M_20 + w * M_30;
            yc = x * M_01 + y * M_11 + z * M_21 + w * M_31;
            zc = x * M_02 + y * M_12 + z * M_22 + w * M_32;
            wc = x * M_03 + y * M_13 + z * M_23 + w * M_33;
        }

        if (wc < 0.001f) wc = 0.001f;
        float inv_w = 1.0f / wc;
        float ndc_x = xc * inv_w;
        float ndc_y = yc * inv_w;
        float ndc_z = zc * inv_w;

        /* XS and YS are signed 12.4 fixed point relative to viewport centre (Figure 12) */
        float rel_x = ndc_x * args->vp_sx;
        float rel_y = -ndc_y * args->vp_sy;
        float screen_z = ndc_z * args->vp_sz + args->vp_az;
        if (screen_z < 0.0f) screen_z = 0.0f;
        else if (screen_z > 1.0f) screen_z = 1.0f;

        int32_t fix_x = (int32_t)(rel_x * 16.0f);
        int32_t fix_y = (int32_t)(rel_y * 16.0f);
        if (fix_x > 32767) fix_x = 32767;
        else if (fix_x < -32768) fix_x = -32768;
        if (fix_y > 32767) fix_y = 32767;
        else if (fix_y < -32768) fix_y = -32768;
        out_verts[i].xs = LE16((v3d_u16)(int16_t)fix_x);
        out_verts[i].ys = LE16((v3d_u16)(int16_t)fix_y);
        swap_float32_into(&out_verts[i].zs, screen_z);
        swap_float32_into(&out_verts[i].inv_w, inv_w);

        /* Varyings: v0=T, v1=S, v2=R, v3=G, v4=B */
        float v0 = 0.0f, v1 = 0.0f, v2 = 1.0f, v3 = 1.0f, v4 = 1.0f;
        if (args->shape & (D_SR_COMBINED | D_SR_SMOOTH_ALPHATEST))
        {
            float s = read_le_float(&args->texbuf[i * 4 + 0]);
            float t = read_le_float(&args->texbuf[i * 4 + 1]);
            float r = read_le_float(&args->texbuf[i * 4 + 2]);
            float g = read_le_float(&args->texbuf[i * 4 + 3]);
            float b = args->texbuf2 ? read_le_float(&args->texbuf2[i * 2 + 0]) : 1.0f;
            v0 = t;
            v1 = s;
            v2 = r;
            v3 = g;
            v4 = b;
        }
        else if (args->shape & D_SR_TEXTURED)
        {
            float s = read_le_float(&args->texbuf[i * 2 + 0]);
            float t = read_le_float(&args->texbuf[i * 2 + 1]);
            v0 = t;
            v1 = s;
            v2 = 1.0f;
            v3 = 1.0f;
            v4 = 1.0f;
        }
        else if (args->shape & D_SR_SMOOTH)
        {
            float r = read_le_float(&args->texbuf[i * 4 + 0]);
            float g = read_le_float(&args->texbuf[i * 4 + 1]);
            float b = read_le_float(&args->texbuf[i * 4 + 2]);
            v0 = 0.0f;
            v1 = 0.0f;
            v2 = r;
            v3 = g;
            v4 = b;
        }
        else
        {
            v0 = 0.0f;
            v1 = 0.0f;
            v2 = ((args->flat_color >> 16) & 0xFF) * (1.0f / 255.0f);
            v3 = ((args->flat_color >> 8) & 0xFF) * (1.0f / 255.0f);
            v4 = (args->flat_color & 0xFF) * (1.0f / 255.0f);
        }

        swap_float32_into(&out_verts[i].v0, v0);
        swap_float32_into(&out_verts[i].v1, v1);
        swap_float32_into(&out_verts[i].v2, v2);
        swap_float32_into(&out_verts[i].v3, v3);
        swap_float32_into(&out_verts[i].v4, v4);
    }

    /* 2. Claim memory for NV Shader Record (16-byte aligned) */
    sb->used = (sb->used + 15) & ~15;
    VC4NVShaderRecord* rec = (VC4NVShaderRecord*)v3d_cl_claim_fast(
        device, sm, sb, sizeof(VC4NVShaderRecord), &context->frame);
    if (!rec) return;

    /* Flag bits (Table 46):
     * Bit 0: Fragment Shader is single threaded (1)
     * Bit 1: Point Size included (0)
     * Bit 2: Enable Clipping (0)
     * Bit 3: Clip Coordinates header included (0)
     * => 0x01 (Form 3: No Clip Header, No Point Size, 32 bytes) */
    rec->flag_bits = 0x01;
    rec->vertex_stride_bytes = (v3d_u8)sizeof(VC4NVShadedVertex); /* 32 bytes */
    rec->uniform_num = 0;
    rec->varying_num = 5;
    rec->fshader_code_addr = LE32(args->fshader_code_addr);
    rec->fshader_uniform_addr = LE32(args->unif_frag_addr);
    rec->vertex_data_addr = LE32((ULONG)out_verts);

    /* 3. Emit packets into binning list */
    v3d_static_buffer* buffer = context->current_buf;

    /* NV Shader State (opcode 65): shader record address */
    UBYTE* p65 = (UBYTE*)v3d_buffer_claim_memory(buffer, 5);
    if (!p65) return;
    p65[0] = v3d_OP_NV_SHADER_STATE;
    *(ULONG*)(p65 + 1) = LE32((ULONG)rec);

    /* 4. Emit primitives */
    if (args->idxbuf != NULL)
    {
        /* Opcode 32: Indexed Primitive List */
        UBYTE* p32 = (UBYTE*)v3d_buffer_claim_memory(buffer, 14);
        if (!p32) return;
        p32[0] = v3d_OP_INDEXED_PRIM_LIST;
        p32[1] = (UBYTE)((args->primType & 0x0F) | (v3d_INDEX_TYPE_16_BIT << 4));
        *(ULONG*)(p32 + 2) = LE32((ULONG)args->nidx);
        *(ULONG*)(p32 + 6) = LE32((ULONG)args->idxbuf);
        *(ULONG*)(p32 + 10) = LE32((ULONG)(count - 1));
    }
    else
    {
        /* Opcode 33: Vertex Array Primitives */
        int chunk = (args->chunk_size > 0) ? args->chunk_size : count;
        int i;
        for (i = 0; i < count; i += chunk)
        {
            int n = (i + chunk <= count) ? chunk : (count - i);
            VertexArrayPrims(context, args->primType, (ULONG)n, (ULONG)i);
        }
    }

    D(("vc4: emit_prim verts=%ld prim=%ld chunk=%ld fshader=0x%08lx unif=0x%08lx srec=0x%08lx\n",
        (LONG)count, (LONG)args->primType, (LONG)args->chunk_size,
        (ULONG)args->fshader_code_addr, (ULONG)args->unif_frag_addr, (ULONG)rec));
}

