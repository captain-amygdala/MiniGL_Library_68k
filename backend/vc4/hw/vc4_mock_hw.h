/*
 * VideoCore IV (VC4) Mock Hardware & In-Memory Simulation
 * Enables testing, verification, and execution without physical Pi/PiStorm hardware.
 */

#ifndef VC4_MOCK_HW_H
#define VC4_MOCK_HW_H

#include "../include/v3d_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t binning_jobs;
    uint32_t render_jobs;
    uint32_t binning_bytes;
    uint32_t render_bytes;
    uint32_t packets_total;
    uint32_t primitives_total;
    uint32_t tiles_total;
    uint32_t shader_records_seen;
    uint32_t errors_detected;
    uint32_t pixels_rasterized;
} V3DMockStats;

/* Check if mock hardware mode is active */
int  v3d_mock_is_active(void);

/* Initialize mock register block and activate mock hardware */
void v3d_mock_init(void);

/* Process binning command list in mock mode */
void v3d_mock_process_binning(v3d_address start, v3d_address end);

/* Process render command list in mock mode */
void v3d_mock_process_render(v3d_address start, v3d_address end);

/* Query mock simulation statistics */
void v3d_mock_get_stats(V3DMockStats *stats);

/* Reset mock simulation statistics */
void v3d_mock_reset_stats(void);

/* Get human-readable name of VC4 opcode */
const char* v3d_mock_opcode_name(uint8_t op);

/* Dump a 32-bit RGBA framebuffer to a standard P6 PPM image */
int v3d_mock_dump_ppm(const char* filename, const void* fb, int width, int height);

/* Get the last rendered framebuffer address and dimensions */
void* v3d_mock_get_framebuffer(int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif /* VC4_MOCK_HW_H */
