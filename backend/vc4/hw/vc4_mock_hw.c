/*
 * VideoCore IV (VC4) Mock Hardware & In-Memory Simulation
 * Includes Control-List Validation, Coordinate Shader Matrix Transform,
 * Perspective-Correct Texture Software Rasterizer, and Depth Buffer.
 */

#include "vc4_mock_hw.h"
#include "vc4_regs.h"
#include "vc4_hw.h"
#include "vc4_debug.h"
#include "../include/v3d_types.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define MOCK_MAX_RECORDED_PRIMS 1024
#define MOCK_MAX_VERTICES_PER_PRIM 128
#define MOCK_MAX_FB_DIM 1024

typedef struct {
    float x, y, z, w;       /* Screen space x, y, z and clip-space w */
    float inv_w;            /* 1.0 / w */
    float u, v;             /* Texture coordinates */
    float r, g, b, a;       /* Per-vertex colors */
} MockScreenVertex;

typedef struct {
    uint32_t mode;          /* V3D_PRIM_TRIANGLES, TRIANGLEFAN, TRIANGLESTRIP */
    uint32_t count;         /* Number of vertices */
    uint32_t first_index;   /* First vertex index */

    /* Vertex buffer attributes */
    uint32_t pos_addr;
    uint32_t pos_stride;
    uint32_t tex_addr;
    uint32_t tex_stride;
    uint32_t tex2_addr;
    uint32_t tex2_stride;
    uint32_t varyings;      /* 6 = combined, 2 = textured, 4 = smooth, 0 = flat */

    /* Texture info */
    int      has_texture;
    uint32_t tex_base_addr;
    uint32_t tex_width;
    uint32_t tex_height;

    /* Transform state (from coordinate uniforms) */
    float scale_p;
    float scale_p_y;
    float M[4][4];
    float z_scale;
    float z_offset;
    float vp_center_x;
    float vp_center_y;

    /* Legacy mock vertex buffer support (from render_mock_triangle.c) */
    int is_legacy_mock;
} MockRecordedPrim;

static uint32_t s_mock_v3d_regs[0x1000 / sizeof(uint32_t)] __attribute__((aligned(16)));
static int s_mock_active = 0;
static V3DMockStats s_mock_stats;

/* Recorded state during Binning */
static MockRecordedPrim s_recorded_prims[MOCK_MAX_RECORDED_PRIMS];
static int s_recorded_prim_count = 0;

/* Latched state during Binning packet parsing */
static uint32_t s_curr_pos_addr = 0;
static uint32_t s_curr_pos_stride = 12;
static uint32_t s_curr_tex_addr = 0;
static uint32_t s_curr_tex_stride = 0;
static uint32_t s_curr_tex2_addr = 0;
static uint32_t s_curr_tex2_stride = 0;
static uint32_t s_curr_varyings = 0;

static int      s_curr_has_texture = 0;
static uint32_t s_curr_tex_base_addr = 0;
static uint32_t s_curr_tex_width = 0;
static uint32_t s_curr_tex_height = 0;

static float s_curr_scale_p = 256.0f;
static float s_curr_scale_p_y = 256.0f;
static float s_curr_M[4][4];
static float s_curr_z_scale = 0.5f;
static float s_curr_z_offset = 0.5f;
static float s_curr_vp_center_x = 128.0f;
static float s_curr_vp_center_y = 128.0f;
static int   s_curr_is_legacy_mock = 0;

/* Framebuffer target configured during Render */
static uint32_t s_target_fb_addr = 0;
static uint16_t s_target_fb_width = 0;
static uint16_t s_target_fb_height = 0;
static uint32_t s_target_clear_color = 0;

/* Depth buffer for hidden surface removal */
static float s_depth_buffer[512 * 512];

static inline float read_le_float(const void* src)
{
    uint32_t u = LE32(*(const uint32_t*)src);
    float f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

int v3d_mock_is_active(void)
{
    return s_mock_active;
}

void v3d_mock_init(void)
{
    memset(s_mock_v3d_regs, 0, sizeof(s_mock_v3d_regs));
    memset(&s_mock_stats, 0, sizeof(s_mock_stats));
    s_recorded_prim_count = 0;

    s_curr_pos_addr = 0;
    s_curr_pos_stride = 12;
    s_curr_tex_addr = 0;
    s_curr_tex_stride = 0;
    s_curr_tex2_addr = 0;
    s_curr_tex2_stride = 0;
    s_curr_varyings = 0;
    s_curr_has_texture = 0;
    s_curr_tex_base_addr = 0;
    s_curr_tex_width = 0;
    s_curr_tex_height = 0;

    s_curr_scale_p = 256.0f;
    s_curr_scale_p_y = 256.0f;
    memset(s_curr_M, 0, sizeof(s_curr_M));
    s_curr_M[0][0] = 1.0f; s_curr_M[1][1] = 1.0f; s_curr_M[2][2] = 1.0f; s_curr_M[3][3] = 1.0f;
    s_curr_z_scale = 0.5f;
    s_curr_z_offset = 0.5f;
    s_curr_vp_center_x = 128.0f;
    s_curr_vp_center_y = 128.0f;
    s_curr_is_legacy_mock = 0;

    s_target_fb_addr = 0;
    s_target_fb_width = 0;
    s_target_fb_height = 0;
    s_target_clear_color = 0;

    g_vc4_v3d_base = (ULONG)&s_mock_v3d_regs[0];
    s_mock_active = 1;

    /* Initialize core identity: VC4 V3D 2.1 ('V3D' = 0x02443356 in LE) */
    V3D_IDENT0 = LE32(0x02443356);
    V3D_IDENT1 = LE32(0x00000001);
    V3D_IDENT2 = LE32(0x00000002);

    V3D_BFC   = 0;
    V3D_RFC   = 0;
    V3D_CT0CS = 0;
    V3D_CT1CS = 0;
    V3D_SRQCS = 0;

    D(("v3d_mock: hardware simulation initialized at base 0x%08lx\n", g_vc4_v3d_base));
}

void v3d_mock_reset_stats(void)
{
    memset(&s_mock_stats, 0, sizeof(s_mock_stats));
    s_recorded_prim_count = 0;
}

void v3d_mock_get_stats(V3DMockStats *stats)
{
    if (stats) {
        *stats = s_mock_stats;
    }
}

void* v3d_mock_get_framebuffer(int *width, int *height)
{
    if (width)  *width = (int)s_target_fb_width;
    if (height) *height = (int)s_target_fb_height;
    return (void*)s_target_fb_addr;
}

const char* v3d_mock_opcode_name(uint8_t op)
{
    switch (op) {
    case v3d_OP_HALT:                                              return "HALT";
    case v3d_OP_NOP:                                               return "NOP";
    case v3d_OP_FLUSH:                                             return "FLUSH";
    case v3d_OP_FLUSH_ALL:                                         return "FLUSH_ALL";
    case v3d_OP_START_TILE_BINNING:                                return "START_TILE_BINNING";
    case v3d_OP_INCREMENT_SEMAPHORE:                               return "INCREMENT_SEMAPHORE";
    case v3d_OP_WAIT_ON_SEMAPHORE:                                 return "WAIT_ON_SEMAPHORE";
    case v3d_OP_BRANCH:                                            return "BRANCH";
    case v3d_OP_BRANCH_TO_SUB_LIST:                                return "BRANCH_TO_SUB_LIST";
    case v3d_OP_RETURN_FROM_SUB_LIST:                              return "RETURN_FROM_SUB_LIST";
    case v3d_OP_STORE_MULTI_SAMPLE_RESOLVED_TILE_COLOR_BUFFER:     return "STORE_MS_RESOLVED_COLOR";
    case v3d_OP_STORE_MULTI_SAMPLE_RESOLVED_TILE_COLOR_BUFFER_AND_EOF: return "STORE_MS_RESOLVED_COLOR_EOF";
    case v3d_OP_STORE_TILE_BUFFER_GENERAL:                         return "STORE_TILE_BUFFER_GENERAL";
    case v3d_OP_LOAD_TILE_BUFFER_GENERAL:                          return "LOAD_TILE_BUFFER_GENERAL";
    case v3d_OP_INDEXED_PRIM_LIST:                                 return "INDEXED_PRIM_LIST";
    case v3d_OP_VERTEX_ARRAY_PRIMS:                                return "VERTEX_ARRAY_PRIMS";
    case v3d_OP_PRIM_LIST_FORMAT:                                  return "PRIM_LIST_FORMAT";
    case v3d_OP_GL_SHADER_STATE:                                   return "GL_SHADER_STATE";
    case v3d_OP_NV_SHADER_STATE:                                   return "NV_SHADER_STATE";
    case v3d_OP_CFG_BITS:                                          return "CFG_BITS";
    case v3d_OP_FLAT_SHADE_FLAGS:                                  return "FLAT_SHADE_FLAGS";
    case v3d_OP_POINT_SIZE:                                        return "POINT_SIZE";
    case v3d_OP_LINE_WIDTH:                                        return "LINE_WIDTH";
    case v3d_OP_RHT_X_BOUNDARY:                                    return "RHT_X_BOUNDARY";
    case v3d_OP_DEPTH_OFFSET:                                      return "DEPTH_OFFSET";
    case v3d_OP_CLIPWINDOW:                                        return "CLIPWINDOW";
    case v3d_OP_VIEWPORT_OFFSET:                                   return "VIEWPORT_OFFSET";
    case v3d_OP_Z_MIN_AND_MAX_CLIPPING_PLANES:                     return "Z_MIN_AND_MAX_CLIPPING_PLANES";
    case v3d_OP_CLIPPER_XY_SCALING:                                return "CLIPPER_XY_SCALING";
    case v3d_OP_CLIPPER_Z_SCALE_AND_OFFSET:                        return "CLIPPER_Z_SCALE_AND_OFFSET";
    case v3d_OP_TILE_BINNING_MODE_CFG:                             return "TILE_BINNING_MODE_CFG";
    case v3d_OP_TILE_RENDERING_MODE_CFG:                           return "TILE_RENDERING_MODE_CFG";
    case v3d_OP_TILE_RENDERING_CLEAR_COLORS:                       return "TILE_RENDERING_CLEAR_COLORS";
    case v3d_OP_TILE_COORDINATES:                                  return "TILE_COORDINATES";
    default:                                                       return "UNKNOWN";
    }
}

static int get_packet_len(uint8_t op)
{
    switch (op) {
    case v3d_OP_HALT:                                              return 1;
    case v3d_OP_NOP:                                               return 1;
    case v3d_OP_FLUSH:                                             return 1;
    case v3d_OP_FLUSH_ALL:                                         return 1;
    case v3d_OP_START_TILE_BINNING:                                return 1;
    case v3d_OP_INCREMENT_SEMAPHORE:                               return 1;
    case v3d_OP_WAIT_ON_SEMAPHORE:                                 return 1;
    case v3d_OP_BRANCH:                                            return 5;
    case v3d_OP_BRANCH_TO_SUB_LIST:                                return 5;
    case v3d_OP_RETURN_FROM_SUB_LIST:                              return 1;
    case v3d_OP_STORE_MULTI_SAMPLE_RESOLVED_TILE_COLOR_BUFFER:     return 1;
    case v3d_OP_STORE_MULTI_SAMPLE_RESOLVED_TILE_COLOR_BUFFER_AND_EOF: return 1;
    case v3d_OP_STORE_TILE_BUFFER_GENERAL:                         return 7;
    case v3d_OP_LOAD_TILE_BUFFER_GENERAL:                          return 7;
    case v3d_OP_INDEXED_PRIM_LIST:                                 return 14;
    case v3d_OP_VERTEX_ARRAY_PRIMS:                                return 10;
    case v3d_OP_PRIM_LIST_FORMAT:                                  return 2;
    case v3d_OP_GL_SHADER_STATE:                                   return 5;
    case v3d_OP_NV_SHADER_STATE:                                   return 5;
    case v3d_OP_CFG_BITS:                                          return 4;
    case v3d_OP_FLAT_SHADE_FLAGS:                                  return 5;
    case v3d_OP_POINT_SIZE:                                        return 5;
    case v3d_OP_LINE_WIDTH:                                        return 5;
    case v3d_OP_RHT_X_BOUNDARY:                                    return 3;
    case v3d_OP_DEPTH_OFFSET:                                      return 5;
    case v3d_OP_CLIPWINDOW:                                        return 9;
    case v3d_OP_VIEWPORT_OFFSET:                                   return 5;
    case v3d_OP_Z_MIN_AND_MAX_CLIPPING_PLANES:                     return 9;
    case v3d_OP_CLIPPER_XY_SCALING:                                return 9;
    case v3d_OP_CLIPPER_Z_SCALE_AND_OFFSET:                        return 9;
    case v3d_OP_TILE_BINNING_MODE_CFG:                             return 16;
    case v3d_OP_TILE_RENDERING_MODE_CFG:                           return 11;
    case v3d_OP_TILE_RENDERING_CLEAR_COLORS:                       return 14;
    case v3d_OP_TILE_COORDINATES:                                  return 3;
    default:                                                       return -1;
    }
}

void v3d_mock_process_binning(v3d_address start, v3d_address end)
{
    const uint8_t *p = (const uint8_t*)start;
    const uint8_t *limit = (const uint8_t*)end;
    uint32_t bytes = (uint32_t)(limit - p);

    s_mock_stats.binning_jobs++;
    s_mock_stats.binning_bytes += bytes;

    while (p < limit) {
        uint8_t op = *p;
        int len = get_packet_len(op);

        if (len <= 0 || p + len > limit) {
            D(("v3d_mock: invalid binning packet %u at offset %ld\n", (unsigned)op, (long)(p - (const uint8_t*)start)));
            s_mock_stats.errors_detected++;
            break;
        }

        s_mock_stats.packets_total++;

        if (op == v3d_OP_VIEWPORT_OFFSET) {
            s_curr_vp_center_x = (float)(int16_t)LE16(*(const uint16_t*)(p + 1)) / 16.0f;
            s_curr_vp_center_y = (float)(int16_t)LE16(*(const uint16_t*)(p + 3)) / 16.0f;
        }
        else if (op == v3d_OP_NV_SHADER_STATE) {
            s_mock_stats.shader_records_seen++;
            uint32_t srec_addr = LE32(*(const uint32_t*)(p + 1));
            if (srec_addr != 0) {
                const uint8_t* rec = (const uint8_t*)srec_addr;
                s_curr_pos_stride = rec[1];
                s_curr_pos_addr = LE32(*(const uint32_t*)(rec + 12));
                s_curr_tex_addr = 0;
                s_curr_tex_stride = 0;
                s_curr_tex2_addr = 0;
                s_curr_tex2_stride = 0;
                s_curr_is_legacy_mock = 1;
                s_curr_has_texture = 0;
                s_curr_varyings = 4;
            }
        }
        else if (op == v3d_OP_GL_SHADER_STATE) {
            s_mock_stats.shader_records_seen++;
            uint32_t raw = LE32(*(const uint32_t*)(p + 1));
            uint32_t srec_addr = raw & ~0x1FU;
            uint32_t num_attrs = raw & 0x1FU;
            if (srec_addr != 0) {
                s_curr_is_legacy_mock = 0;
                const v3d_gl_shader_state_record* rec = (const v3d_gl_shader_state_record*)srec_addr;
                s_curr_varyings = rec->number_of_varyings_in_fragment_shader;

                /* Unpack coordinate shader uniforms (MVP matrix, viewport scales, depth range) */
                uint32_t unif_coord = LE32(rec->coordinate_shader_uniforms_address);
                if (unif_coord != 0) {
                    const float* pu = (const float*)unif_coord;
                    s_curr_scale_p   = read_le_float(&pu[0]);
                    s_curr_scale_p_y = read_le_float(&pu[1]);
                    s_curr_M[0][0] = read_le_float(&pu[2]);
                    s_curr_M[1][0] = read_le_float(&pu[3]);
                    s_curr_M[2][0] = read_le_float(&pu[4]);
                    s_curr_M[3][0] = read_le_float(&pu[5]);

                    s_curr_M[0][1] = read_le_float(&pu[6]);
                    s_curr_M[1][1] = read_le_float(&pu[7]);
                    s_curr_M[2][1] = read_le_float(&pu[8]);
                    s_curr_M[3][1] = read_le_float(&pu[9]);

                    s_curr_M[0][2] = read_le_float(&pu[10]);
                    s_curr_M[1][2] = read_le_float(&pu[11]);
                    s_curr_M[2][2] = read_le_float(&pu[12]);
                    s_curr_M[3][2] = read_le_float(&pu[13]);

                    s_curr_M[0][3] = read_le_float(&pu[14]);
                    s_curr_M[1][3] = read_le_float(&pu[15]);
                    s_curr_M[2][3] = read_le_float(&pu[16]);
                    s_curr_M[3][3] = read_le_float(&pu[17]);

                    s_curr_z_scale  = read_le_float(&pu[18]);
                    s_curr_z_offset = read_le_float(&pu[19]);
                }

                /* Unpack fragment shader uniforms (TMU texture state) */
                s_curr_has_texture = 0;
                uint32_t unif_frag = LE32(rec->fragment_shader_uniforms_address);
                if (unif_frag != 0 && (s_curr_varyings == 6 || s_curr_varyings == 2)) {
                    uint32_t p0_raw = LE32(*(const uint32_t*)unif_frag);
                    v3d_tmu_config_parameter_0 p0;
                    memcpy(&p0, &p0_raw, sizeof(p0));
                    uint32_t ts_addr = p0.texture_state_address_rshift_4 << 4;
                    if (ts_addr != 0) {
                        v3d_texture_shader_state ts;
                        memcpy(&ts, (const void*)ts_addr, sizeof(ts));
                        uint32_t* sw = (uint32_t*)&ts;
                        sw[0] = LE32(sw[0]);
                        sw[1] = LE32(sw[1]);
                        sw[2] = LE32(sw[2]);
                        sw[3] = LE32(sw[3]);
                        sw[4] = LE32(sw[4]);

                        s_curr_tex_base_addr = ts.texture_base_pointer_rshift_6 << 6;
                        s_curr_tex_width = ts.image_width_lo | (ts.image_width_hi << 6);
                        s_curr_tex_height = ts.image_height;
                        if (s_curr_tex_width > 0 && s_curr_tex_height > 0 && s_curr_tex_base_addr != 0) {
                            s_curr_has_texture = 1;
                        }
                    }
                }

                /* Attribute 0: position */
                const v3d_gl_shader_state_attribute_record* a0 =
                    (const v3d_gl_shader_state_attribute_record*)(srec_addr + 36);
                s_curr_pos_addr = LE32(a0->address);
                s_curr_pos_stride = a0->stride;

                /* Attribute 1: texcoord / color */
                if (num_attrs >= 2) {
                    const v3d_gl_shader_state_attribute_record* a1 =
                        (const v3d_gl_shader_state_attribute_record*)(srec_addr + 36 + 16);
                    s_curr_tex_addr = LE32(a1->address);
                    s_curr_tex_stride = a1->stride;
                } else {
                    s_curr_tex_addr = 0;
                    s_curr_tex_stride = 0;
                }

                /* Attribute 2: second texcoord / color2 */
                if (num_attrs >= 3) {
                    const v3d_gl_shader_state_attribute_record* a2 =
                        (const v3d_gl_shader_state_attribute_record*)(srec_addr + 36 + 32);
                    s_curr_tex2_addr = LE32(a2->address);
                    s_curr_tex2_stride = a2->stride;
                } else {
                    s_curr_tex2_addr = 0;
                    s_curr_tex2_stride = 0;
                }
            }
        }
        else if (op == v3d_OP_VERTEX_ARRAY_PRIMS) {
            s_mock_stats.primitives_total++;
            uint8_t mode = p[1];
            uint32_t count = LE32(*(const uint32_t*)(p + 2));
            uint32_t index = LE32(*(const uint32_t*)(p + 6));
            if (s_recorded_prim_count < MOCK_MAX_RECORDED_PRIMS) {
                MockRecordedPrim* rp = &s_recorded_prims[s_recorded_prim_count++];
                rp->mode = mode;
                rp->count = count;
                rp->first_index = index;
                rp->is_legacy_mock = s_curr_is_legacy_mock;
                rp->pos_addr = s_curr_pos_addr;
                rp->pos_stride = s_curr_pos_stride ? s_curr_pos_stride : 12;
                rp->tex_addr = s_curr_tex_addr;
                rp->tex_stride = s_curr_tex_stride;
                rp->tex2_addr = s_curr_tex2_addr;
                rp->tex2_stride = s_curr_tex2_stride;
                rp->varyings = s_curr_varyings;
                rp->has_texture = s_curr_has_texture;
                rp->tex_base_addr = s_curr_tex_base_addr;
                rp->tex_width = s_curr_tex_width;
                rp->tex_height = s_curr_tex_height;
                rp->scale_p = s_curr_scale_p;
                rp->scale_p_y = s_curr_scale_p_y;
                memcpy(rp->M, s_curr_M, sizeof(s_curr_M));
                rp->z_scale = s_curr_z_scale;
                rp->z_offset = s_curr_z_offset;
                rp->vp_center_x = s_curr_vp_center_x;
                rp->vp_center_y = s_curr_vp_center_y;
            }
        }
        else if (op == v3d_OP_HALT) {
            p += len;
            break;
        }

        p += len;
    }

    /* Advance Binning Flush Count */
    uint32_t bfc = LE32(V3D_BFC);
    bfc = (bfc + 1) & 0xFF;
    V3D_BFC = LE32(bfc);

    if (s_mock_stats.binning_jobs <= 2) {
        printf("[MOCK_BINNING] job=%lu: %lu bytes, BFC=%lu, recorded_prims=%d\n",
               (ULONG)s_mock_stats.binning_jobs, (ULONG)bytes, (ULONG)bfc, s_recorded_prim_count);
    }
}

/* Transform a vertex from object space to screen space */
static void transform_vertex(const MockRecordedPrim* prim, uint32_t v_idx, MockScreenVertex* out)
{
    if (prim->is_legacy_mock) {
        /* Legacy mock format: 12.4 fixed point x, y, and float z, w, r, g, b, a */
        const uint8_t* p = (const uint8_t*)(prim->pos_addr + v_idx * prim->pos_stride);
        int16_t fx = (int16_t)LE16(*(const int16_t*)(p + 0));
        int16_t fy = (int16_t)LE16(*(const int16_t*)(p + 2));
        out->x = (float)fx / 16.0f;
        out->y = (float)fy / 16.0f;
        out->z = read_le_float(p + 4);
        out->w = read_le_float(p + 8);
        out->inv_w = (out->w != 0.0f) ? (1.0f / out->w) : 1.0f;
        out->r = read_le_float(p + 12);
        out->g = read_le_float(p + 16);
        out->b = read_le_float(p + 20);
        out->a = read_le_float(p + 24);
        out->u = 0.0f;
        out->v = 0.0f;
        return;
    }

    /* Standard MiniGL vertex: object-space coordinates transformed by CombinedMatrix */
    const uint8_t* ppos = (const uint8_t*)(prim->pos_addr + (prim->first_index + v_idx) * prim->pos_stride);
    float x = read_le_float(ppos + 0);
    float y = read_le_float(ppos + 4);
    float z = read_le_float(ppos + 8);
    float w = (prim->pos_stride >= 16) ? read_le_float(ppos + 12) : 1.0f;

    /* Matrix multiply: x_s = x M_00 + y M_10 + z M_20 + w M_30 */
    float xs = x * prim->M[0][0] + y * prim->M[1][0] + z * prim->M[2][0] + w * prim->M[3][0];
    float ys = x * prim->M[0][1] + y * prim->M[1][1] + z * prim->M[2][1] + w * prim->M[3][1];
    float zs = x * prim->M[0][2] + y * prim->M[1][2] + z * prim->M[2][2] + w * prim->M[3][2];
    float ws = x * prim->M[0][3] + y * prim->M[1][3] + z * prim->M[2][3] + w * prim->M[3][3];

    float inv_w = (ws != 0.0f) ? (1.0f / ws) : 1.0f;
    float xp = (xs * inv_w) * 0.5f * prim->scale_p;
    float yp = (-ys * inv_w) * 0.5f * prim->scale_p_y;
    float z_sc = (zs * inv_w) * prim->z_scale + prim->z_offset;

    out->x = prim->vp_center_x + xp;
    out->y = prim->vp_center_y + yp;
    out->z = z_sc;
    out->w = ws;
    out->inv_w = inv_w;

    /* Unpack varyings */
    if (prim->varyings == 6) {
        /* Combined: Attr 1 is u, v, r, g. Attr 2 is b, a. */
        const uint8_t* pt1 = (const uint8_t*)(prim->tex_addr + (prim->first_index + v_idx) * prim->tex_stride);
        out->u = read_le_float(pt1 + 0);
        out->v = read_le_float(pt1 + 4);
        out->r = read_le_float(pt1 + 8);
        out->g = read_le_float(pt1 + 12);
        if (prim->tex2_addr != 0) {
            const uint8_t* pt2 = (const uint8_t*)(prim->tex2_addr + (prim->first_index + v_idx) * prim->tex2_stride);
            out->b = read_le_float(pt2 + 0);
            out->a = read_le_float(pt2 + 4);
        } else {
            out->b = 1.0f; out->a = 1.0f;
        }
    } else if (prim->varyings == 2) {
        /* Textured uncolored: Attr 1 is u, v */
        const uint8_t* pt1 = (const uint8_t*)(prim->tex_addr + (prim->first_index + v_idx) * prim->tex_stride);
        out->u = read_le_float(pt1 + 0);
        out->v = read_le_float(pt1 + 4);
        out->r = 1.0f; out->g = 1.0f; out->b = 1.0f; out->a = 1.0f;
    } else if (prim->varyings == 4) {
        /* Smooth untextured: Attr 1 is r, g, b, a */
        const uint8_t* pt1 = (const uint8_t*)(prim->tex_addr + (prim->first_index + v_idx) * prim->tex_stride);
        out->u = 0.0f; out->v = 0.0f;
        out->r = read_le_float(pt1 + 0);
        out->g = read_le_float(pt1 + 4);
        out->b = read_le_float(pt1 + 8);
        out->a = read_le_float(pt1 + 12);
    } else {
        /* Flat untextured */
        out->u = 0.0f; out->v = 0.0f;
        out->r = 1.0f; out->g = 1.0f; out->b = 1.0f; out->a = 1.0f;
    }
}

/* Perspective-correct triangle rasterization with depth testing and texture mapping */
static void rasterize_triangle_textured(uint8_t* fb, float* zbuf, int fb_w, int fb_h,
                                        const MockRecordedPrim* prim,
                                        const MockScreenVertex* va,
                                        const MockScreenVertex* vb,
                                        const MockScreenVertex* vc)
{
    float xa = va->x, ya = va->y;
    float xb = vb->x, yb = vb->y;
    float xc = vc->x, yc = vc->y;

    float area = (xb - xa) * (yc - ya) - (yb - ya) * (xc - xa);
    if (fabsf(area) < 0.0001f) return;
    float inv_area = 1.0f / area;

    /* Bounding box */
    float min_x_f = xa < xb ? (xa < xc ? xa : xc) : (xb < xc ? xb : xc);
    float max_x_f = xa > xb ? (xa > xc ? xa : xc) : (xb > xc ? xb : xc);
    float min_y_f = ya < yb ? (ya < yc ? ya : yc) : (yb < yc ? yb : yc);
    float max_y_f = ya > yb ? (ya > yc ? ya : yc) : (yb > yc ? yb : yc);

    int min_x = (int)min_x_f; if (min_x < 0) min_x = 0;
    int max_x = (int)(max_x_f + 0.999f); if (max_x >= fb_w) max_x = fb_w - 1;
    int min_y = (int)min_y_f; if (min_y < 0) min_y = 0;
    int max_y = (int)(max_y_f + 0.999f); if (max_y >= fb_h) max_y = fb_h - 1;

    for (int y = min_y; y <= max_y; y++) {
        float py = (float)y + 0.5f;
        for (int x = min_x; x <= max_x; x++) {
            float px = (float)x + 0.5f;

            float w0 = ((xb - px) * (yc - py) - (yb - py) * (xc - px)) * inv_area;
            float w1 = ((xc - px) * (ya - py) - (yc - py) * (xa - px)) * inv_area;
            float w2 = 1.0f - w0 - w1;

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                continue;

            /* Interpolate Z in screen space */
            float z = w0 * va->z + w1 * vb->z + w2 * vc->z;

            /* Depth Test (GL_LESS) */
            int pixel_idx = y * fb_w + x;
            if (zbuf != NULL) {
                if (z < 0.0f || z > 1.0f || z >= zbuf[pixel_idx])
                    continue;
                zbuf[pixel_idx] = z;
            }

            /* Perspective-correct varying interpolation */
            float interp_inv_w = w0 * va->inv_w + w1 * vb->inv_w + w2 * vc->inv_w;
            float W = (interp_inv_w > 1e-7f) ? (1.0f / interp_inv_w) : 1.0f;

            float r = (w0 * va->r * va->inv_w + w1 * vb->r * vb->inv_w + w2 * vc->r * vc->inv_w) * W;
            float g = (w0 * va->g * va->inv_w + w1 * vb->g * vb->inv_w + w2 * vc->g * vc->inv_w) * W;
            float b = (w0 * va->b * va->inv_w + w1 * vb->b * vb->inv_w + w2 * vc->b * vc->inv_w) * W;
            float a = (w0 * va->a * va->inv_w + w1 * vb->a * vb->inv_w + w2 * vc->a * vc->inv_w) * W;

            /* Texture sampling */
            if (prim->has_texture && prim->tex_base_addr != 0) {
                float u = (w0 * va->u * va->inv_w + w1 * vb->u * vb->inv_w + w2 * vc->u * vc->inv_w) * W;
                float v = (w0 * va->v * va->inv_w + w1 * vb->v * vb->inv_w + w2 * vc->v * vc->inv_w) * W;

                /* Wrap mode (GL_REPEAT) */
                u = u - floorf(u);
                v = v - floorf(v);

                int tx = (int)(u * (float)(prim->tex_width - 1) + 0.5f);
                if (tx < 0) tx = 0; else if (tx >= (int)prim->tex_width) tx = prim->tex_width - 1;

                int ty = (int)((1.0f - v) * (float)(prim->tex_height - 1) + 0.5f);
                if (ty < 0) ty = 0; else if (ty >= (int)prim->tex_height) ty = prim->tex_height - 1;

                const uint8_t* tex_ptr = (const uint8_t*)(prim->tex_base_addr + (ty * prim->tex_width + tx) * 4);
                float tr = (float)tex_ptr[0] / 255.0f;
                float tg = (float)tex_ptr[1] / 255.0f;
                float tb = (float)tex_ptr[2] / 255.0f;
                float ta = (float)tex_ptr[3] / 255.0f;

                /* GL_MODULATE */
                r *= tr;
                g *= tg;
                b *= tb;
                a *= ta;
            }

            int ir = (int)(r * 255.0f + 0.5f); if (ir < 0) ir = 0; else if (ir > 255) ir = 255;
            int ig = (int)(g * 255.0f + 0.5f); if (ig < 0) ig = 0; else if (ig > 255) ig = 255;
            int ib = (int)(b * 255.0f + 0.5f); if (ib < 0) ib = 0; else if (ib > 255) ib = 255;
            int ia = (int)(a * 255.0f + 0.5f); if (ia < 0) ia = 0; else if (ia > 255) ia = 255;

            uint8_t* dst = fb + pixel_idx * 4;
            dst[0] = (uint8_t)ir;
            dst[1] = (uint8_t)ig;
            dst[2] = (uint8_t)ib;
            dst[3] = (uint8_t)ia;

            s_mock_stats.pixels_rasterized++;
        }
    }
}

void v3d_mock_process_render(v3d_address start, v3d_address end)
{
    const uint8_t *p = (const uint8_t*)start;
    const uint8_t *limit = (const uint8_t*)end;
    uint32_t bytes = (uint32_t)(limit - p);

    s_mock_stats.render_jobs++;
    s_mock_stats.render_bytes += bytes;

    while (p < limit) {
        uint8_t op = *p;
        int len = get_packet_len(op);

        if (len <= 0 || p + len > limit) {
            D(("v3d_mock: invalid render packet %u at offset %ld\n", (unsigned)op, (long)(p - (const uint8_t*)start)));
            s_mock_stats.errors_detected++;
            break;
        }

        s_mock_stats.packets_total++;

        if (op == v3d_OP_TILE_RENDERING_MODE_CFG) {
            s_target_fb_addr = LE32(*(const uint32_t*)(p + 1));
            s_target_fb_width = LE16(*(const uint16_t*)(p + 5));
            s_target_fb_height = LE16(*(const uint16_t*)(p + 7));
        }
        else if (op == v3d_OP_STORE_TILE_BUFFER_GENERAL) {
            uint32_t addr = LE32(*(const uint32_t*)(p + 3));
            if (addr != 0) {
                s_target_fb_addr = addr;
            }
        }
        else if (op == v3d_OP_TILE_RENDERING_CLEAR_COLORS) {
            s_target_clear_color = LE32(*(const uint32_t*)(p + 1));
        }
        else if (op == v3d_OP_TILE_COORDINATES) {
            s_mock_stats.tiles_total++;
        }
        else if (op == v3d_OP_HALT) {
            p += len;
            break;
        }

        p += len;
    }

    /* If a valid framebuffer was configured, execute software rasterization */
    if (s_target_fb_addr != 0 && s_target_fb_width > 0 && s_target_fb_height > 0) {
        uint8_t* fb = (uint8_t*)s_target_fb_addr;
        int w = (int)s_target_fb_width;
        int h = (int)s_target_fb_height;

        /* 1. Clear Framebuffer and Depth buffer */
        uint8_t cr = (uint8_t)(s_target_clear_color & 0xFF);
        uint8_t cg = (uint8_t)((s_target_clear_color >> 8) & 0xFF);
        uint8_t cb = (uint8_t)((s_target_clear_color >> 16) & 0xFF);
        uint8_t ca = (uint8_t)((s_target_clear_color >> 24) & 0xFF);

        for (int i = 0; i < w * h; i++) {
            fb[i * 4 + 0] = cr;
            fb[i * 4 + 1] = cg;
            fb[i * 4 + 2] = cb;
            fb[i * 4 + 3] = ca;
        }

        int max_depth_pixels = w * h;
        if (max_depth_pixels > 512 * 512) max_depth_pixels = 512 * 512;
        for (int i = 0; i < max_depth_pixels; i++) {
            s_depth_buffer[i] = 1.0f;
        }

        /* 2. Rasterize recorded primitives */
        MockScreenVertex s_verts[MOCK_MAX_VERTICES_PER_PRIM];

        for (int i = 0; i < s_recorded_prim_count; i++) {
            MockRecordedPrim* prim = &s_recorded_prims[i];
            if (prim->pos_addr == 0 || prim->count == 0)
                continue;

            uint32_t count = prim->count;
            if (count > MOCK_MAX_VERTICES_PER_PRIM)
                count = MOCK_MAX_VERTICES_PER_PRIM;

            for (uint32_t v = 0; v < count; v++) {
                transform_vertex(prim, v, &s_verts[v]);
            }

            if (prim->mode == V3D_PRIM_TRIANGLES) {
                uint32_t num_tris = count / 3;
                for (uint32_t t = 0; t < num_tris; t++) {
                    rasterize_triangle_textured(fb, s_depth_buffer, w, h, prim,
                        &s_verts[t * 3 + 0], &s_verts[t * 3 + 1], &s_verts[t * 3 + 2]);
                }
            } else if (prim->mode == V3D_PRIM_TRIANGLEFAN) {
                /* Fan decomposition: (0, 1, 2), (0, 2, 3), (0, 3, 4), ... */
                for (uint32_t t = 1; t + 1 < count; t++) {
                    rasterize_triangle_textured(fb, s_depth_buffer, w, h, prim,
                        &s_verts[0], &s_verts[t], &s_verts[t + 1]);
                }
            } else if (prim->mode == V3D_PRIM_TRIANGLESTRIP) {
                /* Strip decomposition with alternating winding */
                for (uint32_t t = 0; t + 2 < count; t++) {
                    if (t & 1) {
                        rasterize_triangle_textured(fb, s_depth_buffer, w, h, prim,
                            &s_verts[t + 1], &s_verts[t], &s_verts[t + 2]);
                    } else {
                        rasterize_triangle_textured(fb, s_depth_buffer, w, h, prim,
                            &s_verts[t], &s_verts[t + 1], &s_verts[t + 2]);
                    }
                }
            }
        }
    }

    s_recorded_prim_count = 0;

    /* Advance Render Frame Count */
    uint32_t rfc = LE32(V3D_RFC);
    rfc = (rfc + 1) & 0xFF;
    V3D_RFC = LE32(rfc);

    D(("v3d_mock: render job #%lu complete: %lu bytes, RFC=%lu, pixels=%lu\n",
       (ULONG)s_mock_stats.render_jobs, (ULONG)bytes, (ULONG)rfc, (ULONG)s_mock_stats.pixels_rasterized));
}

int v3d_mock_dump_ppm(const char* filename, const void* fb, int width, int height)
{
    if (!filename || !fb || width <= 0 || height <= 0)
        return -1;

    FILE* f = fopen(filename, "wb");
    if (!f) return -2;

    fprintf(f, "P6\n%d %d\n255\n", width, height);

    const uint8_t* src = (const uint8_t*)fb;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            fputc(src[0], f); /* R */
            fputc(src[1], f); /* G */
            fputc(src[2], f); /* B */
            src += 4;         /* RGBA */
        }
    }

    fclose(f);
    return 0;
}
