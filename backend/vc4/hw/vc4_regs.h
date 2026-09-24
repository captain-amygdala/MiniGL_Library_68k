/*
 * VideoCore IV (VC4 / V3D 2.x) Register Definitions
 * Target: BCM2837 (Raspberry Pi 3) / BCM2835 (Pi 1/Zero)
 */

#ifndef VC4_REGS_H
#define VC4_REGS_H

#include <exec/types.h>

#ifndef _STDINT_H
typedef unsigned long uint32_t;
#endif

extern ULONG g_vc4_v3d_base;

#define VC4_V3D_BASE g_vc4_v3d_base

#define V3D_IDENT0       *(volatile uint32_t*)(VC4_V3D_BASE + 0x000)
#define V3D_IDENT1       *(volatile uint32_t*)(VC4_V3D_BASE + 0x004)
#define V3D_IDENT2       *(volatile uint32_t*)(VC4_V3D_BASE + 0x008)
#define V3D_SCRATCH      *(volatile uint32_t*)(VC4_V3D_BASE + 0x010)
#define V3D_L2CACTL      *(volatile uint32_t*)(VC4_V3D_BASE + 0x020)
#define V3D_SLCACTL      *(volatile uint32_t*)(VC4_V3D_BASE + 0x024)
#define V3D_INTCTL       *(volatile uint32_t*)(VC4_V3D_BASE + 0x030)
#define V3D_INTENA       *(volatile uint32_t*)(VC4_V3D_BASE + 0x034)
#define V3D_INTDIS       *(volatile uint32_t*)(VC4_V3D_BASE + 0x038)

#define V3D_CTL_INT_STS  V3D_INTCTL
#define V3D_CTL_INT_CLR  V3D_INTCTL

#define V3D_CT0CS        *(volatile uint32_t*)(VC4_V3D_BASE + 0x100)
#define V3D_CT1CS        *(volatile uint32_t*)(VC4_V3D_BASE + 0x104)
#define V3D_CT0EA        *(volatile uint32_t*)(VC4_V3D_BASE + 0x108)
#define V3D_CT1EA        *(volatile uint32_t*)(VC4_V3D_BASE + 0x10c)
#define V3D_CT0CA        *(volatile uint32_t*)(VC4_V3D_BASE + 0x110)
#define V3D_CT1CA        *(volatile uint32_t*)(VC4_V3D_BASE + 0x114)
#define V3D_CT00RA0      *(volatile uint32_t*)(VC4_V3D_BASE + 0x118)
#define V3D_CT01RA0      *(volatile uint32_t*)(VC4_V3D_BASE + 0x11c)
#define V3D_CT0LC        *(volatile uint32_t*)(VC4_V3D_BASE + 0x120)
#define V3D_CT1LC        *(volatile uint32_t*)(VC4_V3D_BASE + 0x124)
#define V3D_CT0PC        *(volatile uint32_t*)(VC4_V3D_BASE + 0x128)
#define V3D_CT1PC        *(volatile uint32_t*)(VC4_V3D_BASE + 0x12c)
#define V3D_PCS          *(volatile uint32_t*)(VC4_V3D_BASE + 0x130)
#define V3D_BFC          *(volatile uint32_t*)(VC4_V3D_BASE + 0x134)
#define V3D_RFC          *(volatile uint32_t*)(VC4_V3D_BASE + 0x138)

#define V3D_BPCA         *(volatile uint32_t*)(VC4_V3D_BASE + 0x300)
#define V3D_BPCS         *(volatile uint32_t*)(VC4_V3D_BASE + 0x304)
#define V3D_BPOA         *(volatile uint32_t*)(VC4_V3D_BASE + 0x308)
#define V3D_BPOS         *(volatile uint32_t*)(VC4_V3D_BASE + 0x30c)
#define V3D_BXCF         *(volatile uint32_t*)(VC4_V3D_BASE + 0x310)

#define V3D_SQRSV0       *(volatile uint32_t*)(VC4_V3D_BASE + 0x410)
#define V3D_SQRSV1       *(volatile uint32_t*)(VC4_V3D_BASE + 0x414)
#define V3D_SQCNTL       *(volatile uint32_t*)(VC4_V3D_BASE + 0x418)
#define V3D_SRQPC        *(volatile uint32_t*)(VC4_V3D_BASE + 0x430)
#define V3D_SRQUA        *(volatile uint32_t*)(VC4_V3D_BASE + 0x434)
#define V3D_SRQUL        *(volatile uint32_t*)(VC4_V3D_BASE + 0x438)
#define V3D_SRQCS        *(volatile uint32_t*)(VC4_V3D_BASE + 0x43c)
#define V3D_VPACNTL      *(volatile uint32_t*)(VC4_V3D_BASE + 0x500)
#define V3D_VPMBASE      *(volatile uint32_t*)(VC4_V3D_BASE + 0x504)

#define V3D_PCTRC        *(volatile uint32_t*)(VC4_V3D_BASE + 0x670)
#define V3D_PCTRE        *(volatile uint32_t*)(VC4_V3D_BASE + 0x674)
#define V3D_PCTR(i)      *(volatile uint32_t*)(VC4_V3D_BASE + 0x680 + (i)*8)
#define V3D_PCTRS(i)     *(volatile uint32_t*)(VC4_V3D_BASE + 0x684 + (i)*8)

#define V3D_DBGE         *(volatile uint32_t*)(VC4_V3D_BASE + 0xF00)
#define V3D_FDBGO        *(volatile uint32_t*)(VC4_V3D_BASE + 0xF04)
#define V3D_FDBGB        *(volatile uint32_t*)(VC4_V3D_BASE + 0xF08)
#define V3D_FDBGR        *(volatile uint32_t*)(VC4_V3D_BASE + 0xF0C)
#define V3D_FDBGS        *(volatile uint32_t*)(VC4_V3D_BASE + 0xF10)
#define V3D_ERRSTAT      *(volatile uint32_t*)(VC4_V3D_BASE + 0xF20)

/* Status / Flag Bits */
#define V3D_INT_FRDONE   0x00000001
#define V3D_INT_FLDONE   0x00000002
#define V3D_INT_OUTOMEM  0x00000004
#define V3D_INT_SPILLUSE 0x00000008

#define V3D_CTNCS_CTERR  0x08000000
#define V3D_CTNCS_CTRSTA 0x00800000
#define V3D_T0QTS_ENABLE 0x02000000

#define V3D_BFC_FLUSH_COUNT_MASK 0xFF
#define V3D_RFC_FRAME_COUNT_MASK 0xFF

#endif /* VC4_REGS_H */
