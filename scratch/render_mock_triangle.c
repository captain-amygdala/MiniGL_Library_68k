#include <stdio.h>
#include <string.h>
#include <proto/exec.h>

#include "v3d_types.h"
#include "v3d_device.h"
#include "v3d_context.h"
#include "v3d_commands.h"
#include "v3d_frame.h"
#include "v3d_shader_assembler.h"
#include "vc4_hw.h"
#include "vc4_mock_hw.h"
#include "vc4_submit_timeout.h"

typedef struct {
    uint8_t  flag_bits;
    uint8_t  vertex_stride_bytes;
    uint8_t  uniform_num;
    uint8_t  varying_num;
    uint32_t fshader_code_addr;
    uint32_t fshader_uniform_addr;
    uint32_t vertex_data_addr;
} __attribute__((packed)) VC4ShaderRecord;

typedef struct {
    int16_t x, y;
    float   z, w;
    float   r, g, b, a;
} VC4Vertex;

int main(void)
{
    const int WIDTH = 256;
    const int HEIGHT = 256;

    printf("=====================================================\n");
    printf("   VC4 Mock Hardware 3D Triangle Rendering Pipeline  \n");
    printf("=====================================================\n");

    /* 1. Device Init */
    V3DDevice device;
    memset(&device, 0, sizeof(device));
    device.sysbase = *(struct ExecBase**)4L;

    if (v3d_init(&device) != 0) {
        printf("FAILED: v3d_init\n");
        return 1;
    }
    printf("[1/7] V3D 2.1 Device initialized (mock active: %d)\n", v3d_mock_is_active());

    /* 2. Assemble Built-in Shaders */
    if (!v3d_assemble_builtin_shaders(&device)) {
        printf("FAILED: v3d_assemble_builtin_shaders\n");
        v3d_free(&device);
        return 1;
    }
    printf("[2/7] 114 VC4 shaders assembled in M68k RAM\n");

    /* 3. Context Init */
    V3DContext context;
    memset(&context, 0, sizeof(context));
    if (v3d_context_init(&context, &device, WIDTH, HEIGHT) != 0) {
        printf("FAILED: v3d_context_init\n");
        v3d_free(&device);
        return 1;
    }
    printf("[3/7] Context initialized for %dx%d (tiles: %lux%lu)\n",
           WIDTH, HEIGHT, context.frame.tilesX, context.frame.tilesY);

    /* 4. Allocate 32-bit RGBA Framebuffer */
    v3d_mem fb_mem;
    if (v3d_mem_alloc(&device, &fb_mem, WIDTH * HEIGHT * 4) != 0) {
        printf("FAILED: alloc framebuffer\n");
        v3d_context_free(&device, &context);
        v3d_free(&device);
        return 1;
    }
    context.framebuffer = (v3d_address)(ULONG)fb_mem.hostptr;
    printf("[4/7] Framebuffer allocated at 0x%08lx (%d KB)\n",
           (ULONG)fb_mem.hostptr, (WIDTH * HEIGHT * 4) / 1024);

    /* 5. Geometry: Smooth-shaded RGB Triangle */
    VC4Vertex vertices[3] = {
        /* Top: Red (128, 28) */
        { 128 << 4,  28 << 4, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f },
        /* Bottom-Left: Green (28, 228) */
        {  28 << 4, 228 << 4, 0.5f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f },
        /* Bottom-Right: Blue (228, 228) */
        { 228 << 4, 228 << 4, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f }
    };

    VC4ShaderRecord srec;
    memset(&srec, 0, sizeof(srec));
    srec.flag_bits = 0;
    srec.vertex_stride_bytes = sizeof(VC4Vertex);
    srec.uniform_num = 0;
    srec.varying_num = 4;
    srec.fshader_code_addr = LE32((uint32_t)v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_SMOOTH].instructions);
    srec.fshader_uniform_addr = 0;
    srec.vertex_data_addr = LE32((uint32_t)&vertices[0]);

    /* 6. Build Binning Control List */
    v3d_static_buffer* bin_buf = &context.binning_buf[0];
    v3d_cl_reset(&context.binning_mem[0], bin_buf);
    SetBuffer(&context, bin_buf);

    TileBinningModeCfg(&context, v3d_TILE_ALLOCATION_INITIAL_BLOCK_SIZE_64B,
                       v3d_TILE_ALLOCATION_BLOCK_SIZE_64B,
                       1, V3D_INTERNAL_BPP_32, 0, 0, WIDTH, HEIGHT);
    ClipWindow(&context, 0, 0, WIDTH, HEIGHT);
    ClipperXYScaling(&context, WIDTH, HEIGHT);
    CfgBits(&context, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    DoCommand(&context, v3d_OP_START_TILE_BINNING);

    /* Emit NV Shader State */
    uint8_t* p = (uint8_t*)v3d_buffer_claim_memory(context.current_buf, 5);
    p[0] = v3d_OP_NV_SHADER_STATE;
    *(uint32_t*)(p + 1) = LE32((uint32_t)&srec);

    PrimListFormat(&context, v3d_LIST_TRIANGLES, 0);
    VertexArrayPrims(&context, V3D_PRIM_TRIANGLES, 3, 0);
    DoCommand(&context, v3d_OP_FLUSH);

    ULONG bin_start = (ULONG)bin_buf->start;
    ULONG bin_end = bin_start + bin_buf->used;

    /* 7. Build Render Control List */
    v3d_static_buffer* rnd_buf = &context.render_buf[0];
    v3d_cl_reset(&context.render_mem[0], rnd_buf);
    SetBuffer(&context, rnd_buf);

    TileRenderingModeCFGCommon(&context, 1, WIDTH, HEIGHT, 32, 0, 0, 0, 0, 0, 0);
    /* Clear Color: Dark Slate Blue (0xFF241C18 in RGBA) */
    TileRenderingModeCFGClearColorsPart1(&context, 0, 0xFF241C18);

    /* Emit tiles (4x4 = 16 tiles) */
    int tiles_x = (WIDTH + 63) / 64;
    int tiles_y = (HEIGHT + 63) / 64;
    for (int ty = 0; ty < tiles_y; ty++) {
        for (int tx = 0; tx < tiles_x; tx++) {
            TileCoordinates(&context, tx, ty);
            if (tx == tiles_x - 1 && ty == tiles_y - 1) {
                DoCommand(&context, v3d_OP_STORE_MULTI_SAMPLE_RESOLVED_TILE_COLOR_BUFFER_AND_EOF);
            } else {
                DoCommand(&context, v3d_OP_STORE_MULTI_SAMPLE_RESOLVED_TILE_COLOR_BUFFER);
            }
        }
    }

    ULONG rnd_start = (ULONG)rnd_buf->start;
    ULONG rnd_end = rnd_start + rnd_buf->used;
    printf("[5/7] Control Lists generated: Binning=%lu B, Render=%lu B\n",
           (ULONG)bin_buf->used, (ULONG)rnd_buf->used);

    /* 8. Execute Binning & Render via Mock Hardware */
    printf("[6/7] Submitting hardware jobs to VC4 GPU...\n");
    v3d_u8 bflush = v3d_submit_binning(&device, &context.frame, bin_start, bin_end,
                                       (v3d_address)(ULONG)context.tile_alloc_mem.hostptr,
                                       (v3d_u32)context.tile_alloc_mem.size,
                                       (v3d_address)(ULONG)context.tile_state_mem.hostptr);
    v3d_wait_binning_timeout(&device, &context.frame, bflush);

    v3d_u8 rframe = v3d_submit_render(rnd_start, rnd_end);
    v3d_wait_render_timeout(rframe);

    V3DMockStats stats;
    v3d_mock_get_stats(&stats);
    printf("      -> Binning Flush completed (BFC=%u)\n", (unsigned)v3d_get_binning_flush_count());
    printf("      -> Render Frame completed (RFC=%u)\n", (unsigned)v3d_get_render_frame_count());
    printf("      -> Pixels rasterized: %lu\n", (ULONG)stats.pixels_rasterized);

    /* 9. Save Rendered Frame to PPM */
    const char* ppm_path = "scratch/mock_triangle.ppm";
    printf("[7/7] Saving rendered frame to %s...\n", ppm_path);
    if (v3d_mock_dump_ppm(ppm_path, fb_mem.hostptr, WIDTH, HEIGHT) == 0) {
        printf("      -> PPM image successfully written!\n");
    } else {
        printf("      -> WARNING: could not write PPM file\n");
    }

    /* Teardown */
    v3d_mem_free(&device, &fb_mem);
    v3d_context_free(&device, &context);
    v3d_free(&device);

    printf("=====================================================\n");
    printf("   Render Finished Successfully!                     \n");
    printf("=====================================================\n");
    return 0;
}
