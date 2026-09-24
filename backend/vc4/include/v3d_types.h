/*
 * VideoCore IV (VC4 / V3D 2.x) base types for PiStorm3D
 * Compatible with AmigaOS 68k big-endian architecture
 */

#ifndef VC4_TYPES_H
#define VC4_TYPES_H

#include <exec/types.h>
#include <string.h>

typedef unsigned char  v3d_u8;
typedef unsigned short v3d_u16;
typedef unsigned int   v3d_u32;
typedef int            v3d_i32;

#ifndef _STDINT_H
typedef unsigned long  uint32_t;
typedef unsigned long  uintptr_t;
#endif

typedef signed int         v3d_int;
typedef unsigned long long v3d_u64;
typedef unsigned long      v3d_uint;
typedef float              v3d_float;
typedef unsigned int       v3d_address;
typedef unsigned int       v3d_uintptr;
typedef unsigned short     v3d_f187;
typedef unsigned int       v3d_u14_8;
typedef unsigned short     v3d_u4_8;
typedef signed short       v3d_s8_8;
typedef v3d_u64            v3d_qpu_instruction;

/* LE32/LE16/NOP - Endianness primitives (68k Big-Endian <-> VC4 Little-Endian) */
#ifdef __VBCC__
ULONG LE32(__reg("d0")ULONG) = "\trol.w\t#8,d0\n\tswap\td0\n\trol.w\t#8,d0\n";
UWORD LE16(__reg("d0")UWORD) = "\trol.w\t#8,d0\n";
VOID NOP(VOID) = "\tnop\n";
#elif defined(__GNUC__)
static __inline__ ULONG LE32(ULONG v)
{
	__asm__ ("rol.w #8,%0\n\tswap %0\n\trol.w #8,%0" : "+d"(v));
	return v;
}
static __inline__ UWORD LE16(UWORD v)
{
	__asm__ ("rol.w #8,%0" : "+d"(v));
	return v;
}
static __inline__ VOID NOP(VOID)
{
	__asm__ volatile ("nop");
}
#else
#error "LE32/LE16/NOP require __VBCC__ or __GNUC__"
#endif

#define V3D_ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))

typedef struct v3d_mem {
    unsigned size;
    unsigned handle;
    unsigned busaddr;   /* GPU bus address */
    void*    hostptr;   /* CPU-side mapped pointer */
} v3d_mem;

enum {
    MEM_FLAG_DISCARDABLE = 1 << 0,
    MEM_FLAG_NORMAL = 0 << 2,
    MEM_FLAG_DIRECT = 1 << 2,
    MEM_FLAG_COHERENT = 2 << 2,
    MEM_FLAG_L1_NONALLOCATING = (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT),
    MEM_FLAG_ZERO = 1 << 4,
    MEM_FLAG_NO_INIT = 1 << 5,
    MEM_FLAG_HINT_PERMALOCK = 1 << 6
};

typedef struct v3d_static_buffer {
    v3d_u8* start;
    int used;
    int capacity;
    v3d_u8 overflowed;
} v3d_static_buffer;

#endif /* VC4_TYPES_H */
