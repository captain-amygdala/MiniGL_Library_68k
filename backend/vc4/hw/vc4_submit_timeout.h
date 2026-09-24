/*
 * VideoCore IV (VC4) Submission and Timeout Handlers
 */

#ifndef VC4_SUBMIT_TIMEOUT_H
#define VC4_SUBMIT_TIMEOUT_H

#include "../include/v3d_types.h"
#include "../include/v3d_device.h"
#include "../include/v3d_frame.h"
#include "vc4_hw.h"

#define V3D_TIMEOUT_ITERATIONS_BINNING 2000000UL
#define V3D_TIMEOUT_ITERATIONS_RENDER  5000000UL

extern v3d_u32 g_v3d_render_timeout_iterations;

v3d_u8 v3d_submit_binning(V3DDevice* device, V3DFrame* frame,
                          v3d_address binningCommandListStart,
                          v3d_address binningCommandListEnd,
                          v3d_address tileAllocation,
                          v3d_u32 tileAllocationSize,
                          v3d_address tileStateData);

v3d_wait_result v3d_wait_binning_timeout(V3DDevice* device, V3DFrame* frame, v3d_u8 lastFlush);

v3d_u8 v3d_submit_render(v3d_address renderCommandListStart,
                         v3d_address renderCommandListEnd);

v3d_wait_result v3d_wait_render_timeout(v3d_u8 lastFrame);

#endif /* VC4_SUBMIT_TIMEOUT_H */
