/*
 * VideoCore IV (VC4) Low-Level Hardware Interface & Packet Definitions
 */

#ifndef VC4_HW_H
#define VC4_HW_H

#include "../include/v3d_types.h"
#include "../include/v3d_clbuf.h"

#ifndef PACKED
#define PACKED __attribute__((packed))
#endif

typedef unsigned char  v3d_bool;
typedef unsigned short v3d_uword;

#ifndef FALSE
#define FALSE 0
#define TRUE  1
#endif

typedef enum {
    v3d_wait_result_success = 0,
    v3d_wait_result_error_detected,
    v3d_wait_result_timed_out,
    v3d_wait_result_timeout = v3d_wait_result_timed_out
} v3d_wait_result;

enum v3d_compare_function {
    V3D_COMPARE_FUNC_NEVER = 0,
    V3D_COMPARE_FUNC_LESS = 1,
    V3D_COMPARE_FUNC_EQUAL = 2,
    V3D_COMPARE_FUNC_LEQUAL = 3,
    V3D_COMPARE_FUNC_GREATER = 4,
    V3D_COMPARE_FUNC_NOTEQUAL = 5,
    V3D_COMPARE_FUNC_GEQUAL = 6,
    V3D_COMPARE_FUNC_ALWAYS = 7
};

enum v3d_primitive {
    V3D_PRIM_POINTS = 0,
    V3D_PRIM_LINES = 1,
    V3D_PRIM_LINELOOP = 2,
    V3D_PRIM_LINESTRIP = 3,
    V3D_PRIM_TRIANGLES = 4,
    V3D_PRIM_TRIANGLESTRIP = 5,
    V3D_PRIM_TRIANGLEFAN = 6,
    /* Aliases with underscore */
    V3D_PRIM_LINE_LOOP = 2,
    V3D_PRIM_LINE_STRIP = 3,
    V3D_PRIM_TRIANGLE_STRIP = 5,
    V3D_PRIM_TRIANGLE_FAN = 6
};

typedef enum v3d_line_rasterization
{
    V3D_LINE_RASTERIZATION_DIAMOND_EXIT = 0,
    V3D_LINE_RASTERIZATION_PERP_END_CAPS = 1,
} v3d_line_rasterization;

typedef enum v3d_blend_mode
{
    V3D_BLEND_MODE_ADD = 0,
    V3D_BLEND_MODE_SUB = 1,
    V3D_BLEND_MODE_RSUB = 2,
    V3D_BLEND_MODE_MIN = 3,
    V3D_BLEND_MODE_MAX = 4,
    V3D_BLEND_MODE_MUL = 5,
    V3D_BLEND_MODE_SCREEN = 6,
    V3D_BLEND_MODE_DARKEN = 7,
    V3D_BLEND_MODE_LIGHTEN = 8,
} v3d_blend_mode;

typedef enum v3d_stencil_op
{
    V3D_STENCIL_OP_ZERO = 0,
    V3D_STENCIL_OP_KEEP = 1,
    V3D_STENCIL_OP_REPLACE = 2,
    V3D_STENCIL_OP_INCR = 3,
    V3D_STENCIL_OP_DECR = 4,
    V3D_STENCIL_OP_INVERT = 5,
} v3d_stencil_op;

enum v3d_blend_factor {
    V3D_BLEND_FACTOR_ZERO = 0,
    V3D_BLEND_FACTOR_ONE = 1,
    V3D_BLEND_FACTOR_SRCCOLOR = 2,
    V3D_BLEND_FACTOR_INVSRCCOLOR = 3,
    V3D_BLEND_FACTOR_DSTCOLOR = 4,
    V3D_BLEND_FACTOR_INVDSTCOLOR = 5,
    V3D_BLEND_FACTOR_SRCALPHA = 6,
    V3D_BLEND_FACTOR_INVSRCALPHA = 7,
    V3D_BLEND_FACTOR_DSTALPHA = 8,
    V3D_BLEND_FACTOR_INVDSTALPHA = 9,
    V3D_BLEND_FACTOR_CONSTCOLOR = 10,
    V3D_BLEND_FACTOR_INVCONSTCOLOR = 11,
    V3D_BLEND_FACTOR_CONSTALPHA = 12,
    V3D_BLEND_FACTOR_INVCONSTALPHA = 13,
    V3D_BLEND_FACTOR_SRCALPHASATURATE = 14,
    /* Underscored aliases */
    V3D_BLEND_FACTOR_SRC_COLOR = 2,
    V3D_BLEND_FACTOR_INV_SRC_COLOR = 3,
    V3D_BLEND_FACTOR_DST_COLOR = 4,
    V3D_BLEND_FACTOR_INV_DST_COLOR = 5,
    V3D_BLEND_FACTOR_SRC_ALPHA = 6,
    V3D_BLEND_FACTOR_INV_SRC_ALPHA = 7,
    V3D_BLEND_FACTOR_DST_ALPHA = 8,
    V3D_BLEND_FACTOR_INV_DST_ALPHA = 9,
    V3D_BLEND_FACTOR_CONST_COLOR = 10,
    V3D_BLEND_FACTOR_INV_CONST_COLOR = 11,
    V3D_BLEND_FACTOR_CONST_ALPHA = 12,
    V3D_BLEND_FACTOR_INV_CONST_ALPHA = 13,
    V3D_BLEND_FACTOR_SRC_ALPHA_SATURATE = 14
};

enum v3d_memory_format {
    V3D_MEMORY_FORMAT_RASTER = 0,
    V3D_MEMORY_FORMAT_TFORMAT = 1,
    V3D_MEMORY_FORMAT_LT = 2,
    V3D_MEMORY_FORMAT_UIF_NO_XOR = 0,
    V3D_MEMORY_FORMAT_UIF_XOR = 1
};

#ifndef V3D_ARRAY_SIZE
#define V3D_ARRAY_SIZE(array) (sizeof((array)) / sizeof((array)[0]))
#endif

#define v3d_ATTRIBUTE_HALF_FLOAT 1
#define v3d_ATTRIBUTE_FLOAT 2
#define v3d_ATTRIBUTE_FIXED 3
#define v3d_ATTRIBUTE_BYTE 4
#define v3d_ATTRIBUTE_SHORT 5
#define v3d_ATTRIBUTE_INT 6
#define v3d_ATTRIBUTE_INT2101010 7

#define v3d_VEC_4 0
#define v3d_VEC_1 1
#define v3d_VEC_2 2
#define v3d_VEC_3 3

#define V3D_FLOAT_TO_U4_8(f) ((v3d_u32)((f) * 256.0f))

static inline v3d_u16 v3d_float_to_f187(float val)
{
    union { float f; v3d_u32 u; } conv;
    conv.f = val;
    return (v3d_u16)(conv.u >> 16);
}

void* v3d_buffer_claim_memory(v3d_static_buffer* buffer, v3d_uintptr size);

#ifdef __GNUC__
static __inline__ void* v3d_buffer_claim_memory_fast(v3d_static_buffer* buffer, v3d_uintptr dataSize)
{
    if (buffer->capacity - buffer->used >= (int)dataSize)
    {
        void* data = buffer->start + buffer->used;
        buffer->used += dataSize;
        return data;
    }
    return v3d_buffer_claim_memory(buffer, dataSize);
}
#endif

/* Macros for buffer claiming and operation allocation */
#define V3D_BUFFER_ALLOC_OPERATION_WITH(claimFunction, bufferAddress, operationId, type, pointerVariableName) \
    type* pointerVariableName = (type*)claimFunction((bufferAddress), sizeof(type));           \
    if (pointerVariableName)                                                                   \
    {                                                                                          \
        v3d_u8* v3d_zp_ = (v3d_u8*)pointerVariableName;                                        \
        v3d_uintptr v3d_zi_;                                                                   \
        for (v3d_zi_ = 0; v3d_zi_ < sizeof(type); v3d_zi_++)                                   \
            v3d_zp_[v3d_zi_] = 0;                                                              \
        pointerVariableName->operation = (operationId);                                        \
    }

#define V3D_BUFFER_ALLOC_OPERATION(bufferAddress, operationId, type, pointerVariableName)      \
    V3D_BUFFER_ALLOC_OPERATION_WITH(v3d_buffer_claim_memory, bufferAddress, operationId, type, pointerVariableName)

#define V3D_BUFFER_ALLOC_STRUCT(bufferAddress, type) ((type*)v3d_buffer_claim_memory(bufferAddress, sizeof(type)))

#define V3D_BUFFER_ALLOC_STRUCTNAME(bufferAddress, type, pointerVariableName)                  \
    type* pointerVariableName = (type*)v3d_buffer_claim_memory(bufferAddress, sizeof(type))

#define V3D_ALIGN(valueToAlign, desiredAlignmentPowerOf2) \
    (((valueToAlign) + (desiredAlignmentPowerOf2) - 1) & ~((desiredAlignmentPowerOf2) - 1))

/*
 * VC4 Control List Opcodes (VC4 Hardware Packets)
 */
#define v3d_OP_HALT                                0
#define v3d_OP_NOP                                 1
#define v3d_OP_FLUSH                               4
#define v3d_OP_FLUSH_ALL                           5
#define v3d_OP_START_TILE_BINNING                  6
#define v3d_OP_INCREMENT_SEMAPHORE                 7
#define v3d_OP_WAIT_ON_SEMAPHORE                   8
#define v3d_OP_BRANCH                              16
#define v3d_OP_BRANCH_TO_SUB_LIST                  17
#define v3d_OP_RETURN_FROM_SUB_LIST                18
#define v3d_OP_STORE_MULTI_SAMPLE_RESOLVED_TILE_COLOR_BUFFER 24
#define v3d_OP_STORE_MULTI_SAMPLE_RESOLVED_TILE_COLOR_BUFFER_AND_EOF 25
#define v3d_OP_STORE_TILE_BUFFER_GENERAL           28
#define v3d_OP_LOAD_TILE_BUFFER_GENERAL            29
#define v3d_OP_INDEXED_PRIM_LIST                   32
#define v3d_OP_VERTEX_ARRAY_PRIMS                  33
#define v3d_OP_INDEX_BUFFER_SETUP                  1   /* NOP on VC4 */
#define v3d_OP_PRIM_LIST_FORMAT                    56
#define v3d_OP_GL_SHADER_STATE                     64
#define v3d_OP_NV_SHADER_STATE                     65
#define v3d_OP_CFG_BITS                            96
#define v3d_OP_FLAT_SHADE_FLAGS                    97
#define v3d_OP_POINT_SIZE                          98
#define v3d_OP_LINE_WIDTH                          99
#define v3d_OP_RHT_X_BOUNDARY                      100
#define v3d_OP_DEPTH_OFFSET                        101
#define v3d_OP_CLIPWINDOW                          102
#define v3d_OP_VIEWPORT_OFFSET                     103
#define v3d_OP_Z_MIN_AND_MAX_CLIPPING_PLANES       104
#define v3d_OP_CLIPPER_XY_SCALING                  105
#define v3d_OP_CLIPPER_Z_SCALE_AND_OFFSET          106
#define v3d_OP_TILE_BINNING_MODE_CFG               112
#define v3d_OP_TILE_RENDERING_MODE_CFG             113
#define v3d_OP_TILE_RENDERING_CLEAR_COLORS         114
#define v3d_OP_TILE_COORDINATES                    115

/* V3D 4.x opcodes mapped to NOP (1) on VC4 for compatibility */
#define v3d_OP_ZERO_ALL_FLAT_SHADE_FLAGS           1
#define v3d_OP_ZERO_ALL_NON_PERSPECTIVE_FLAGS      1
#define v3d_OP_ZERO_ALL_CENTROID_FLAGS             1
#define v3d_OP_FLUSH_VCD_CACHE                     1
#define v3d_OP_TILE_COORDINATES_IMPLICIT           1
#define v3d_OP_END_OF_LOADS                        1
#define v3d_OP_END_OF_TILE_MARKER                  1
#define v3d_OP_END_OF_RENDERING                    1
#define v3d_OP_OCCLUSION_QUERY_COUNTER             1
#define v3d_OP_BLEND_CFG                           1
#define v3d_OP_BLEND_CONSTANT_COLOR                1
#define v3d_OP_COLOR_WRITE_MASKS                   1
#define v3d_OP_CLEAR_TILE_BUFFERS                  1
#define v3d_OP_START_ADDRESS_OF_GENERIC_TILE_LIST  1
#define v3d_OP_BRANCH_TO_IMPLICIT_TILE_LIST        1
#define v3d_OP_SUPERTILE_COORDINATES               1
#define v3d_OP_MULTICORE_RENDERING_TILE_LIST_SET_BASE 1
#define v3d_OP_MULTICORE_RENDERING_SUPERTILE_CFG   1
#define v3d_OP_TILE_LIST_INITIAL_BLOCK_SIZE        1

#define v3d_INDEX_TYPE_8_BIT                       0
#define v3d_INDEX_TYPE_16_BIT                      1
#define v3d_INDEX_TYPE_32_BIT                      2

#define v3d_TILE_ALLOCATION_INITIAL_BLOCK_SIZE_64B 1
#define v3d_TILE_ALLOCATION_BLOCK_SIZE_64B         1

#define v3d_EARLY_Z_DIRECTION_LT_LE                0
#define v3d_EARLY_Z_DIRECTION_GT_GE                1

#define v3d_RENDER_TARGET_MAXIMUM_32BPP            1
#define v3d_RENDER_TARGET_0                        0
#define v3d_NONE                                   8
#define v3d_Z                                      9

#define V3D_DITHER_MODE_NONE                       0
#define V3D_DITHER_MODE_RGBA                       1
#define V3D_DECIMATE_MODE_SAMPLE_0                 0
#define V3D_OUTPUT_IMAGE_FORMAT_RGBA8              0
#define V3D_OUTPUT_IMAGE_FORMAT_D16                1
#define V3D_OUTPUT_IMAGE_FORMAT_D32F               2
#define V3D_INTERNAL_BPP_32                        1
#define V3D_INTERNAL_TYPE_DEPTH16                  1
#define V3D_INTERNAL_TYPE_DEPTH32F                 2
#define V3D_INTERNAL_TYPE_8                        0
#define v3d_LIST_TRIANGLES                         2

#define v3d_SWIZZLE_RED                            0
#define v3d_SWIZZLE_GREEN                          1
#define v3d_SWIZZLE_BLUE                           2
#define v3d_SWIZZLE_ALPHA                          3

#define V3D_WRAP_MODE_REPEAT                       0
#define V3D_WRAP_MODE_CLAMP                        1
#define V3D_WRAP_MODE_BORDER                       2

/*
 * VC4 Command Packet Structs
 */

typedef struct PACKED v3d_flush_vcd_cache
{
    v3d_u8 operation;
} v3d_flush_vcd_cache;

typedef struct PACKED v3d_clipwindow
{
    v3d_u8 operation;
    v3d_u16 clip_window_left_pixel_coordinate;
    v3d_u16 clip_window_bottom_pixel_coordinate;
    v3d_u16 clip_window_width_in_pixels;
    v3d_u16 clip_window_height_in_pixels;
} v3d_clipwindow;

typedef struct PACKED v3d_cfg_bits
{
    v3d_u8 operation;
    v3d_u8 enable_forward_facing_primitive : 1;
    v3d_u8 enable_reverse_facing_primitive : 1;
    v3d_u8 clockwise_primitives : 1;
    v3d_u8 enable_depth_offset : 1;
    v3d_u8 line_rasterization : 1;
    v3d_u8 rasterizer_oversample_mode : 2;
    v3d_u8 direct3d_wireframe_triangles_mode : 1;
    v3d_u8 depth_test_function : 3;
    v3d_u8 z_updates_enable : 1;
    v3d_u8 early_z_enable : 1;
    v3d_u8 early_z_updates_enable : 1;
    v3d_u8 stencil_enable : 1;
    v3d_u8 blend_enable : 1;
    v3d_u8 direct3d_point_fill_mode : 1;
    v3d_u8 direct3d_provoking_vertex : 1;
} v3d_cfg_bits;

typedef struct PACKED v3d_point_size
{
    v3d_u8 operation;
    v3d_u32 point_size;
} v3d_point_size;

typedef struct PACKED v3d_line_width
{
    v3d_u8 operation;
    v3d_u32 line_width;
} v3d_line_width;

typedef struct PACKED v3d_depth_offset
{
    v3d_u8 operation;
    v3d_u16 depth_offset_factor;
    v3d_u16 depth_offset_units;
    v3d_u32 limit;
} v3d_depth_offset;

typedef struct PACKED v3d_clipper_xy_scaling
{
    v3d_u8 operation;
    v3d_u32 viewport_half_width_in_1_16th_of_pixel;
    v3d_u32 viewport_half_height_in_1_16th_of_pixel;
} v3d_clipper_xy_scaling;

typedef struct PACKED v3d_clipper_z_scale_and_offset
{
    v3d_u8 operation;
    v3d_u32 viewport_z_scale;
    v3d_u32 viewport_z_offset;
} v3d_clipper_z_scale_and_offset;

typedef struct PACKED v3d_clipper_z_min_max_clipping_planes
{
    v3d_u8 operation;
    v3d_u32 minimum_zw;
    v3d_u32 maximum_zw;
} v3d_clipper_z_min_max_clipping_planes;

typedef struct PACKED v3d_viewport_offset
{
    v3d_u8 operation;
    v3d_u16 viewport_centre_x_coordinate;
    v3d_u16 viewport_centre_y_coordinate;
} v3d_viewport_offset;

typedef struct PACKED v3d_color_write_masks
{
    v3d_u8 operation;
    v3d_u32 mask;
} v3d_color_write_masks;

typedef struct PACKED v3d_blend_constant_color
{
    v3d_u8 operation;
    v3d_u16 alpha_f16;
    v3d_u16 blue_f16;
    v3d_u16 green_f16;
    v3d_u16 red_f16;
} v3d_blend_constant_color;

typedef struct PACKED v3d_blend_cfg
{
    v3d_u8 operation;
    v3d_u8 alpha_blend_src_factor : 4;
    v3d_u8 alpha_blend_mode : 4;
    v3d_u8 color_blend_mode : 4;
    v3d_u8 alpha_blend_dst_factor : 4;
    v3d_u8 color_blend_dst_factor : 4;
    v3d_u8 color_blend_src_factor : 4;
    v3d_u8 _unused28 : 4;
    v3d_u8 render_target_mask : 4;
} v3d_blend_cfg;

typedef struct PACKED v3d_gl_shader_state
{
    v3d_u8 operation : 8;
    v3d_u32 address_rshift_5 : 27;
    v3d_u32 number_of_attribute_arrays : 5;
} v3d_gl_shader_state;

typedef struct PACKED v3d_vertex_array_prims
{
    v3d_u8 operation;
    v3d_u8 mode;
    v3d_u32 length;
    v3d_u32 index_of_first_vertex;
} v3d_vertex_array_prims;

typedef struct PACKED v3d_indexed_prim_list
{
    v3d_u8 operation;
    v3d_u8 index_type : 2;
    v3d_u8 mode : 6;
    v3d_u32 length_and_restart;
    v3d_u32 index_offset;
} v3d_indexed_prim_list;

typedef struct PACKED v3d_index_buffer_setup
{
    v3d_u8 operation;
    v3d_u32 address;
    v3d_u32 size;
} v3d_index_buffer_setup;

typedef struct PACKED v3d_occlusion_query_counter
{
    v3d_u8 operation;
    v3d_u32 address;
} v3d_occlusion_query_counter;

/* Exact 36-byte Shader Record matching gl/src/draw.c compile-time checks */
typedef struct PACKED v3d_gl_shader_state_record
{
    v3d_u8 base_instance_id_read_by_vertex_shader : 1;
    v3d_u8 instance_id_read_by_vertex_shader : 1;
    v3d_u8 vertex_id_read_by_vertex_shader : 1;
    v3d_u8 base_instance_id_read_by_coordinate_shader : 1;
    v3d_u8 instance_id_read_by_coordinate_shader : 1;
    v3d_u8 vertex_id_read_by_coordinate_shader : 1;
    v3d_u8 enable_clipping : 1;
    v3d_u8 point_size_in_shaded_vertex_data : 1;

    v3d_u8 insert_primitive_id_as_first_varying_to_fragment_shader : 1;
    v3d_u8 any_shader_reads_hardware_written_primitive_id : 1;
    v3d_u8 enable_sample_rate_shading : 1;
    v3d_u8 fragment_shader_uses_real_pixel_centre_w_in_addition_to_centroid_w2 : 1;
    v3d_u8 vertex_shader_has_separate_input_and_output_vpm_blocks : 1;
    v3d_u8 coordinate_shader_has_separate_input_and_output_vpm_blocks : 1;
    v3d_u8 turn_off_early_z_test : 1;
    v3d_u8 fragment_shader_does_z_writes : 1;

    v3d_u8 _unused20 : 4;
    v3d_u8 no_prim_pack : 1;
    v3d_u8 disable_implicit_point_line_varyings : 1;
    v3d_u8 do_scoreboard_wait_on_first_thread_switch : 1;
    v3d_u8 turn_off_scoreboard : 1;

    v3d_u8 number_of_varyings_in_fragment_shader : 8;

    v3d_u8 min_coord_shader_output_segments_required_in_play_in_addition_to_vcm_cache_size : 4;
    v3d_u8 coordinate_shader_output_vpm_segment_size : 4;

    v3d_u8 min_coord_shader_input_segments_required_in_play_minus_one : 4;
    v3d_u8 coordinate_shader_input_vpm_segment_size : 4;

    v3d_u8 min_vertex_shader_output_segments_required_in_play_in_addition_to_vcm_cache_size : 4;
    v3d_u8 vertex_shader_output_vpm_segment_size : 4;

    v3d_u8 min_vertex_shader_input_segments_required_in_play_minus_one : 4;
    v3d_u8 vertex_shader_input_vpm_segment_size : 4;

    v3d_u32 address_of_default_attribute_values : 32;

    v3d_u32 fragment_shader_code_address_rshift_3 : 29;
    v3d_u32 fragment_shader_propagate_nans : 1;
    v3d_u32 fragment_shader_start_in_final_thread_section : 1;
    v3d_u32 fragment_shader_4_way_threadable : 1;

    v3d_u32 fragment_shader_uniforms_address : 32;

    v3d_u32 vertex_shader_code_address_rshift_3 : 29;
    v3d_u32 vertex_shader_propagate_nans : 1;
    v3d_u32 vertex_shader_start_in_final_thread_section : 1;
    v3d_u32 vertex_shader_4_way_threadable : 1;

    v3d_u32 vertex_shader_uniforms_address : 32;

    v3d_u32 coordinate_shader_code_address_rshift_3 : 29;
    v3d_u32 coordinate_shader_propagate_nans : 1;
    v3d_u32 coordinate_shader_start_in_final_thread_section : 1;
    v3d_u32 coordinate_shader_4_way_threadable : 1;

    v3d_u32 coordinate_shader_uniforms_address : 32;
} v3d_gl_shader_state_record;

/* Exact 16-byte Attribute Record matching gl/src/draw.c compile-time checks */
typedef struct PACKED v3d_gl_shader_state_attribute_record
{
    v3d_u32 address : 32;
    v3d_u8 number_of_bytes_minus_1 : 8;
    v3d_u8 stride : 8;
    v3d_u8 vertex_shader_vpm_offset : 8;
    v3d_u8 coordinate_shader_vpm_offset : 8;
    v3d_u32 _reserved[2];
} v3d_gl_shader_state_attribute_record;

/* TMU parameters and texture shader state */
typedef struct PACKED v3d_tmu_config_parameter_0
{
    v3d_u32 texture_state_address_rshift_4 : 28;
    v3d_u32 return_words_of_texture_data : 4;
} v3d_tmu_config_parameter_0;

typedef struct PACKED v3d_tmu_config_parameter_1
{
    v3d_u32 sampler_state_address_rshift_3 : 29;
    v3d_u32 per_pixel_mask_enable : 1;
    v3d_u32 unnormalized_coordinates : 1;
    v3d_u32 output_type_32_bit : 1;
} v3d_tmu_config_parameter_1;

typedef struct PACKED v3d_texture_shader_state
{
    v3d_u32 texture_base_pointer_rshift_6 : 26;
    v3d_u32 reverse_standard_border_color : 1;
    v3d_u32 ahdr : 1;
    v3d_u32 srgb : 1;
    v3d_u32 flip_s_and_t_on_incoming_request : 1;
    v3d_u32 flip_texture_y_axis : 1;
    v3d_u32 flip_texture_x_axis : 1;

    v3d_u32 image_width_lo : 6;
    v3d_u32 array_stride_64_byte_aligned : 26;

    v3d_u32 image_depth_lo : 10;
    v3d_u32 image_height : 14;
    v3d_u32 image_width_hi : 8;

    v3d_u32 base_level : 4;
    v3d_u32 max_level : 4;
    v3d_u32 swizzle_a : 3;
    v3d_u32 swizzle_b : 3;
    v3d_u32 swizzle_g : 3;
    v3d_u32 swizzle_r : 3;
    v3d_u32 extended : 1;
    v3d_u32 texture_type : 7;
    v3d_u32 image_depth_hi : 4;

    v3d_u32 pad2 : 24;
    v3d_u32 uif_xor_disable : 1;
    v3d_u32 level_0_is_strictly_uif : 1;
    v3d_u32 _unused133 : 1;
    v3d_u32 level_0_xor_enable : 1;
    v3d_u32 level_0_ubpad : 4;

    v3d_u32 pad : 32;
} v3d_texture_shader_state;

typedef struct PACKED v3d_sampler_state
{
    v3d_u32 srgb_disable : 1;
    v3d_u32 depth_compare_function : 3;
    v3d_u32 anisotropy_enable : 1;
    v3d_u32 mip_filter_nearest : 1;
    v3d_u32 min_filter_nearest : 1;
    v3d_u32 mag_filter_nearest : 1;

    v3d_u32 max_level_of_detail : 12;
    v3d_u32 min_level_of_detail : 12;

    v3d_u32 fixed_bias : 16;
    v3d_u32 _unused63 : 1;
    v3d_u32 maximum_anisotropy : 2;
    v3d_u32 border_color_mode : 3;
    v3d_u32 wrap_i_border : 1;
    v3d_u32 wrap_r : 3;
    v3d_u32 wrap_t : 3;
    v3d_u32 wrap_s : 3;

    v3d_u32 border_color_word_0;
    v3d_u32 border_color_word_1;
    v3d_u32 border_color_word_2;
    v3d_u32 border_color_word_3;
} v3d_sampler_state;

/* Function prototypes */
void v3d_power_on(void);
void v3d_reset(void);
void v3d_invalidate_caches(void);
v3d_u8 v3d_get_binning_flush_count(void);
v3d_u8 v3d_get_render_frame_count(void);
v3d_wait_result v3d_wait_for_binning_flush(v3d_u8 lastFlush);
v3d_wait_result v3d_wait_for_render_frame(v3d_u8 lastFrame);

void v3d_start_binning_commands(v3d_address binningCommandListStart,
                                v3d_address binningCommandListEnd,
                                v3d_address tileAllocation,
                                v3d_u32 tileAllocationSize,
                                v3d_address tileStateData);

void v3d_start_render_commands(v3d_address renderCommandListStart,
                               v3d_address renderCommandListEnd);

void* v3d_buffer_claim_memory(v3d_static_buffer* buffer, v3d_uintptr size);

v3d_u32 v3d_utile_width(int cpp);
v3d_u32 v3d_utile_height(int cpp);

int v3d_mock_is_active(void);

#endif /* VC4_HW_H */
