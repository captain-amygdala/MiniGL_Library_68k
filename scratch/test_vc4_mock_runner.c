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
    printf("=== VC4 Mock Hardware Simulation Test with Triangle Geometry ===\n");

    /* 1. Initialize Device */
    V3DDevice device;
    memset(&device, 0, sizeof(device));
    device.sysbase = *(struct ExecBase**)4L;

    int ret = v3d_init(&device);
    if (ret != 0) {
        printf("FAILED: v3d_init returned %d\n", ret);
        return 1;
    }

    printf("Device initialized: ver=%lu, rev=%lu, qpu_count=%lu, mock_active=%d\n",
           device.deviceInfo.ver, device.deviceInfo.rev,
           device.deviceInfo.qpu_count, v3d_mock_is_active());

    /* 2. Assemble Built-in Shaders */
    printf("Assembling built-in shaders...\n");
    if (!v3d_assemble_builtin_shaders(&device)) {
        printf("FAILED: v3d_assemble_builtin_shaders failed\n");
        v3d_free(&device);
        return 1;
    }
    printf("Built-in shaders assembled successfully.\n");

    /* 3. Initialize Context (64x64, single tile) */
    V3DContext context;
    memset(&context, 0, sizeof(context));

    printf("Initializing 64x64 context...\n");
    ret = v3d_context_init(&context, &device, 64, 64);
    if (ret != 0) {
        printf("FAILED: v3d_context_init returned %d\n", ret);
        v3d_free(&device);
        return 1;
    }
    printf("Context initialized: tilesX=%lu, tilesY=%lu, zbits=%d\n",
           context.frame.tilesX, context.frame.tilesY, context.zbuffer_bits);

    /* 4. Prepare Triangle Vertex Data & Shader Record */
    VC4Vertex vertices[3] = {
        {  32 << 4,  10 << 4, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f }, /* Top: Red */
        {  10 << 4,  54 << 4, 0.5f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f }, /* Bottom-Left: Green */
        {  54 << 4,  54 << 4, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f }, /* Bottom-Right: Blue */
    };

    VC4ShaderRecord srec;
    memset(&srec, 0, sizeof(srec));
    srec.flag_bits = 0;
    srec.vertex_stride_bytes = sizeof(VC4Vertex);
    srec.uniform_num = 0;
    srec.varying_num = 4;
    srec.fshader_code_addr = LE32((uint32_t)v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED].instructions);
    srec.fshader_uniform_addr = 0;
    srec.vertex_data_addr = LE32((uint32_t)&vertices[0]);

    /* 5. Prepare Binning Control List */
    printf("Emitting binning control list with triangle primitive...\n");
    v3d_static_buffer* bin_buf = &context.binning_buf[0];
    v3d_cl_reset(&context.binning_mem[0], bin_buf);
    SetBuffer(&context, bin_buf);

    TileBinningModeCfg(&context, v3d_TILE_ALLOCATION_INITIAL_BLOCK_SIZE_64B,
                       v3d_TILE_ALLOCATION_BLOCK_SIZE_64B,
                       1, V3D_INTERNAL_BPP_32, 0, 0, 64, 64);
    ClipWindow(&context, 0, 0, 64, 64);
    ClipperXYScaling(&context, 64, 64);
    CfgBits(&context, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    DoCommand(&context, v3d_OP_START_TILE_BINNING);

    /* Emit NV Shader State (Opcode 65) */
    v3d_static_buffer* buf = context.current_buf;
    uint8_t* p = (uint8_t*)v3d_buffer_claim_memory(buf, 5);
    p[0] = v3d_OP_NV_SHADER_STATE;
    *(uint32_t*)(p + 1) = LE32((uint32_t)&srec);

    /* Emit Primitive List Format (Opcode 56) & Vertex Array Prims (Opcode 33) */
    PrimListFormat(&context, v3d_LIST_TRIANGLES, 0);
    VertexArrayPrims(&context, V3D_PRIM_TRIANGLES, 3, 0);

    DoCommand(&context, v3d_OP_FLUSH);

    ULONG bin_start = (ULONG)bin_buf->start;
    ULONG bin_end = bin_start + bin_buf->used;
    printf("Binning list emitted: start=0x%08lx, end=0x%08lx (used=%lu bytes)\n",
           bin_start, bin_end, (ULONG)bin_buf->used);

    /* 6. Prepare Render Control List */
    printf("Emitting render control list...\n");
    v3d_static_buffer* rnd_buf = &context.render_buf[0];
    v3d_cl_reset(&context.render_mem[0], rnd_buf);
    SetBuffer(&context, rnd_buf);

    TileRenderingModeCFGCommon(&context, 1, 64, 64, 32, 0, 0, 0, 0, 0, 0);
    TileRenderingModeCFGClearColorsPart1(&context, 0, 0xFF101010); /* Dark gray clear */
    TileCoordinates(&context, 0, 0);
    DoCommand(&context, v3d_OP_STORE_MULTI_SAMPLE_RESOLVED_TILE_COLOR_BUFFER_AND_EOF);

    ULONG rnd_start = (ULONG)rnd_buf->start;
    ULONG rnd_end = rnd_start + rnd_buf->used;
    printf("Render list emitted: start=0x%08lx, end=0x%08lx (used=%lu bytes)\n",
           rnd_start, rnd_end, (ULONG)rnd_buf->used);

    /* 7. Submit Binning */
    printf("Submitting binning job to mock hardware...\n");
    v3d_u8 lastFlush = v3d_submit_binning(&device, &context.frame,
                                          bin_start, bin_end,
                                          (v3d_address)(ULONG)context.tile_alloc_mem.hostptr,
                                          (v3d_u32)context.tile_alloc_mem.size,
                                          (v3d_address)(ULONG)context.tile_state_mem.hostptr);

    printf("Waiting for binning flush (lastFlush=%u)...\n", (unsigned)lastFlush);
    v3d_wait_result bres = v3d_wait_binning_timeout(&device, &context.frame, lastFlush);
    if (bres != v3d_wait_result_success) {
        printf("FAILED: binning wait returned %d\n", (int)bres);
        v3d_context_free(&device, &context);
        v3d_free(&device);
        return 1;
    }
    printf("Binning completed successfully!\n");

    /* 8. Submit Render */
    printf("Submitting render job to mock hardware...\n");
    v3d_u8 lastFrame = v3d_submit_render(rnd_start, rnd_end);

    printf("Waiting for render frame (lastFrame=%u)...\n", (unsigned)lastFrame);
    v3d_wait_result rres = v3d_wait_render_timeout(lastFrame);
    if (rres != v3d_wait_result_success) {
        printf("FAILED: render wait returned %d\n", (int)rres);
        v3d_context_free(&device, &context);
        v3d_free(&device);
        return 1;
    }
    printf("Render completed successfully!\n");

    /* 9. Inspect Simulation Stats */
    V3DMockStats stats;
    v3d_mock_get_stats(&stats);
    printf("--- Mock Hardware Stats ---\n");
    printf("  Binning jobs:         %lu (%lu bytes)\n", (ULONG)stats.binning_jobs, (ULONG)stats.binning_bytes);
    printf("  Render jobs:          %lu (%lu bytes)\n", (ULONG)stats.render_jobs, (ULONG)stats.render_bytes);
    printf("  Packets total:        %lu\n", (ULONG)stats.packets_total);
    printf("  Shader records seen:  %lu\n", (ULONG)stats.shader_records_seen);
    printf("  Primitives processed: %lu\n", (ULONG)stats.primitives_total);
    printf("  Tiles processed:      %lu\n", (ULONG)stats.tiles_total);
    printf("  Errors detected:      %lu\n", (ULONG)stats.errors_detected);

    if (stats.errors_detected != 0 || stats.primitives_total != 1 || stats.shader_records_seen != 1) {
        printf("FAILED: stats mismatch (errors=%lu, prims=%lu, srec=%lu)\n",
               (ULONG)stats.errors_detected, (ULONG)stats.primitives_total, (ULONG)stats.shader_records_seen);
        v3d_context_free(&device, &context);
        v3d_free(&device);
        return 1;
    }

    /* 10. Teardown */
    printf("Cleaning up context & device...\n");
    v3d_context_free(&device, &context);
    v3d_free(&device);

    printf("=== SUCCESS: All VC4 Binning, Shaders & Rendering Verified in Mock Hardware! ===\n");
    return 0;
}
