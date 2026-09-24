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
     * bit 3..4 (115..116): initial block size
     * bit 5..6 (117..118): block size
     * bit 7 (119): double buffer
     */
    UBYTE flags = (msaa ? 1 : 0) | (1 << 2) | ((taibz & 3) << 3) | ((tabz & 3) << 5) | (db ? (1 << 7) : 0);
    p[15] = flags;
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
    float half_w = (float)width * 0.5f * 16.0f;
    float half_h = (float)height * 0.5f * 16.0f;
    swap_float32_into(p + 1, half_w);
    swap_float32_into(p + 5, half_h);
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
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 9);
    if (!p) return;
    p[0] = v3d_OP_Z_MIN_AND_MAX_CLIPPING_PLANES; /* 104 */
    swap_float32_into(p + 1, minzw);
    swap_float32_into(p + 5, maxzw);
}

void ViewportOffset(V3DContext* context, UWORD width, UWORD coarse_x, UWORD height, UWORD coarse_y)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 5);
    if (!p) return;
    p[0] = v3d_OP_VIEWPORT_OFFSET; /* 103 */
    WORD cx = (WORD)((width / 2) * 16);
    WORD cy = (WORD)((height / 2) * 16);
    *(UWORD*)(p + 1) = LE16(cx);
    *(UWORD*)(p + 3) = LE16(cy);
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

void StoreTileBufferGeneral(V3DContext* context, UBYTE bts, UBYTE mf, UBYTE flipy, ULONG ditm,
                            ULONG decm, ULONG oif, ULONG cbbs, ULONG cr, ULONG rbswap, ULONG strideorub,
                            ULONG height, ULONG address)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 7);
    if (!p) return;
    p[0] = v3d_OP_STORE_TILE_BUFFER_GENERAL; /* 28 */
    p[1] = (UBYTE)((bts & 7) | ((mf & 3) << 4) | ((decm & 3) << 6));
    p[2] = (UBYTE)(oif & 3);
    *(ULONG*)(p + 3) = LE32(address);
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
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 11);
    if (!p) return;

    p[0] = v3d_OP_TILE_RENDERING_MODE_CFG; /* 113 */
    ULONG mem_addr = (ULONG)context->framebuffer;
    *(ULONG*)(p + 1) = LE32(mem_addr);
    *(UWORD*)(p + 5) = LE16(width);
    *(UWORD*)(p + 7) = LE16(height);

    /* Byte 9: format flags */
    UBYTE color_fmt = (maxbpp == 16) ? 0 : 1; /* 1 = RGBA8888, 0 = BGR565 */
    UBYTE mem_fmt = 0; /* 0 = Raster */
    p[9] = (UBYTE)((color_fmt << 2) | (mem_fmt << 6));

    /* Byte 10: early-z and double buffer */
    UBYTE ez_flags = (eztaud ? (1 << 2) : 0) | (ezd ? (1 << 3) : 0);
    p[10] = ez_flags;
}

void TileCoordinates(V3DContext* context, ULONG column, ULONG row)
{
    v3d_static_buffer* buffer = context->current_buf;
    UBYTE* p = (UBYTE*)v3d_buffer_claim_memory(buffer, 3);
    if (!p) return;
    p[0] = v3d_OP_TILE_COORDINATES; /* 115 */
    p[1] = (UBYTE)column;
    p[2] = (UBYTE)row;
}

void StartAddressOfGenericTileList(V3DContext* context, ULONG start, ULONG end)
{
    /*
     * On VC4, render the per-tile command sequence into the RCL:
     * For each tile:
     *   1. TileCoordinates(x, y)
     *   2. Branch to sublist for this tile's binning list (tile_alloc_mem + offset)
     *   3. Store MS Resolved Tile Color Buffer (or EOF on last tile)
     */
    UWORD tile_w = (context->width + 63) / 64;
    UWORD tile_h = (context->height + 63) / 64;
    ULONG tile_alloc_base = (ULONG)context->tile_alloc_mem.busaddr;

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
                ULONG tile_offset = (ULONG)(y * tile_w + x) * 32;
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
