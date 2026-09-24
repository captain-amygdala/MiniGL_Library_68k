/*
 * VideoCore IV (VC4) Device interface
 */

#ifndef VC4_DEVICE_H
#define VC4_DEVICE_H

#include "v3d_types.h"

struct v3d_device_info {
    unsigned char ver;              /* 21 for VC4 (V3D 2.1) */
    unsigned char rev;
    int vpm_size;                    /* VPM size in bytes (16384 on VC4) */
    int qpu_count;                   /* 12 on BCM2835/2837 */
    char has_accumulators;           /* 1 on VC4 (r0-r5) */
};

typedef struct V3DDevice {
    struct ExecBase* sysbase;
    struct DOSBase*  dosbase;
    v3d_u32 BytesAllocated;
    struct v3d_device_info deviceInfo;
} V3DDevice;

int  v3d_init(V3DDevice* device);
void v3d_free(V3DDevice* device);
int  v3d_mem_alloc(V3DDevice* device, v3d_mem* m, unsigned size);
void v3d_mem_free(V3DDevice* device, v3d_mem* m);
int  v3d_qpu_active(V3DDevice* device);

#endif /* VC4_DEVICE_H */
