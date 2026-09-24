/*
 * VideoCore IV (VC4) Vertex formats
 */

#ifndef VC4_VERTEX_H
#define VC4_VERTEX_H

#include "v3d_types.h"

typedef struct V3DVertex {
    float x, y, z, w;
    v3d_u8 r, g, b, a;
    float u0, v0;
    float u1, v1;
} V3DVertex;

typedef struct V3DAttribDesc {
    v3d_u32 offset;
    v3d_u32 stride;
    v3d_u32 maximum_index;
    v3d_u8  read_as_int_uint;
    v3d_u8  normalized_int_type;
    v3d_u8  signed_int_type;
    v3d_u8  vec_size;
    v3d_u8  type;
    v3d_u8  values_read_by_vertex_shader;
    v3d_u8  values_read_by_coordinate_shader;
    v3d_u16 instance_divisor;
} V3DAttribDesc;

#define V3D_ATTRIB_OFFSET(field) ((v3d_u32)((char*)&((V3DVertex*)0)->field - (char*)0))
#define V3D_ATTRIB_STRIDE        ((v3d_u32)sizeof(V3DVertex))

#endif /* VC4_VERTEX_H */
