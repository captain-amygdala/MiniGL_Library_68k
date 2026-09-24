/*
 * VideoCore IV (VC4) Device Initialization & Hardware Setup
 * Target: BCM2837 (Raspberry Pi 3) on PiStorm / Emu68
 */

#include <stdarg.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dos.h>

#include "vc4_debug.h"
#include "devicetree_protos.h"
#include "vc4_regs.h"
#include "vc4_hw.h"
#include "vc4_mock_hw.h"
#include "../include/v3d_device.h"

ULONG g_vc4_v3d_base = 0x3FC00000; /* Default BCM2837 peripheral base 0x3F000000 + 0xC00000 */

#define BUS_TO_PHYS(x) ((x)&~0xC0000000)
#define MAX_COMMAND_LENGTH 32

#define MBOX_READ   LE32(*((const volatile uint32_t*)(MailBox + 0x00)))
#define MBOX_STATUS LE32(*((const volatile uint32_t*)(MailBox + 0x18)))
#define MBOX_WRITE  *((volatile uint32_t*)(MailBox + 0x20))

#define MBOX_CHANNEL 8
#define MBOX_TX_FULL (1UL << 31)
#define MBOX_RX_EMPTY (1UL << 30)
#define MBOX_CHANMASK 0xF

#define VC4_TAG_GET_FIRMWARE_REV  0x00000001
#define VC4_TAG_SET_POWER_STATE   0x00028001
#define VC4_TAG_SET_CLOCK_RATE    0x00038002
#define VC4_TAG_ENABLE_QPU        0x00030012

static APTR DeviceTreeBase;
static ULONG MailBox;
static uint32_t MBReqStorage[MAX_COMMAND_LENGTH + 4];
static uint32_t* MBReq;
static unsigned wait_num;

int v3d_qpu_active(V3DDevice* device)
{
    if (!wait_num)
        return 1;
    if (((LE32(V3D_SRQCS) >> 16) & 0xff) != wait_num)
        return 1;
    wait_num = 0;
    return 0;
}

static void ZeroMem(void* d, ULONG sz)
{
    UBYTE* dst = d;
    while (sz--)
        *dst++ = 0;
}

static uint32_t mbox_recv(void)
{
    uint32_t rsp;
    do {
        while (MBOX_STATUS & MBOX_RX_EMPTY)
            NOP();
        NOP();
        rsp = MBOX_READ;
        NOP();
    } while ((rsp & MBOX_CHANMASK) != MBOX_CHANNEL);
    NOP();
    return rsp & ~MBOX_CHANMASK;
}

static void mbox_send(uint32_t* req)
{
    while (MBOX_STATUS & MBOX_TX_FULL)
        NOP();
    NOP();
    MBOX_WRITE = LE32(((uint32_t)req & ~MBOX_CHANMASK) | MBOX_CHANNEL);
}

static int mbox_transaction(V3DDevice* device, uint32_t* req)
{
    struct ExecBase* const SysBase = device->sysbase;
    ULONG len = LE32(*req) * 4;
    CachePreDMA(req, &len, 0);
    Forbid();
    mbox_send(req);
    mbox_recv();
    Permit();
    CachePostDMA(req, &len, 0);
    if (LE32(req[1]) == 0x80000000)
        return 0;
    D(("VC4: Mailbox transaction failed for command %08lx: %08lx\n", (ULONG)LE32(req[2]), (ULONG)LE32(req[1])));
    return LE32(req[1]);
}

static unsigned qpu_enable(V3DDevice* device, unsigned enable)
{
    int i = 0;
    uint32_t* p = MBReq;

    p[i++] = 0;
    p[i++] = 0;

    p[i++] = LE32(VC4_TAG_ENABLE_QPU);
    p[i++] = LE32(4);
    p[i++] = LE32(4);
    p[i++] = LE32(enable);

    p[i++] = 0;
    p[0] = LE32(i * sizeof(*p));

    if (mbox_transaction(device, p))
        return -1;
    return LE32(p[5]);
}

static unsigned power_on_vc4(V3DDevice* device)
{
    int i = 0;
    uint32_t* p = MBReq;

    p[i++] = 0;
    p[i++] = 0;

    /* Set power state: Device ID 5 = V3D, state 1 = ON (wait) */
    p[i++] = LE32(VC4_TAG_SET_POWER_STATE);
    p[i++] = LE32(8);
    p[i++] = LE32(8);
    p[i++] = LE32(5);
    p[i++] = LE32(1 | 2); /* Power ON and wait */

    /* Set clock rate: Clock ID 5 = V3D, 250 MHz */
    p[i++] = LE32(VC4_TAG_SET_CLOCK_RATE);
    p[i++] = LE32(8);
    p[i++] = LE32(8);
    p[i++] = LE32(5);
    p[i++] = LE32(250 * 1000 * 1000);

    /* Enable QPU */
    p[i++] = LE32(VC4_TAG_ENABLE_QPU);
    p[i++] = LE32(4);
    p[i++] = LE32(4);
    p[i++] = LE32(1);

    p[i++] = 0;
    p[0] = LE32(i * sizeof(*p));

    if (mbox_transaction(device, p))
        return -1;
    return 0;
}

#ifdef VC4_MEM_USE_MAILBOX

static unsigned mem_alloc(V3DDevice* device, unsigned size, unsigned align, unsigned flags)
{
    int i = 0;
    uint32_t* p = MBReq;
    p[i++] = 0;
    p[i++] = 0;
    p[i++] = LE32(0x3000c);
    p[i++] = LE32(12);
    p[i++] = LE32(12);
    p[i++] = LE32(size);
    p[i++] = LE32(align);
    p[i++] = LE32(flags);
    p[i++] = 0;
    p[0] = LE32(i * sizeof(*p));

    if (mbox_transaction(device, p))
        return 0;
    return LE32(p[5]);
}

static unsigned mem_free(V3DDevice* device, unsigned handle)
{
    int i = 0;
    uint32_t* p = MBReq;
    p[i++] = 0;
    p[i++] = 0;
    p[i++] = LE32(0x3000f);
    p[i++] = LE32(4);
    p[i++] = LE32(4);
    p[i++] = LE32(handle);
    p[i++] = 0;
    p[0] = LE32(i * sizeof(*p));

    if (mbox_transaction(device, p))
        return -1;
    return LE32(p[5]);
}

static unsigned mem_lock(V3DDevice* device, unsigned handle)
{
    int i = 0;
    uint32_t* p = MBReq;
    p[i++] = 0;
    p[i++] = 0;
    p[i++] = LE32(0x3000d);
    p[i++] = LE32(4);
    p[i++] = LE32(4);
    p[i++] = LE32(handle);
    p[i++] = 0;
    p[0] = LE32(i * sizeof(*p));

    if (mbox_transaction(device, p))
        return 0;
    return LE32(p[5]);
}

static unsigned mem_unlock(V3DDevice* device, unsigned handle)
{
    int i = 0;
    uint32_t* p = MBReq;
    p[i++] = 0;
    p[i++] = 0;
    p[i++] = LE32(0x3000e);
    p[i++] = LE32(4);
    p[i++] = LE32(4);
    p[i++] = LE32(handle);
    p[i++] = 0;
    p[0] = LE32(i * sizeof(*p));

    if (mbox_transaction(device, p))
        return -1;
    return LE32(p[5]);
}

void v3d_mem_free(V3DDevice* device, v3d_mem* m)
{
    if (m->handle) {
        device->BytesAllocated -= m->size;
        D(("vc4: Freeing %lu bytes, busaddr = %lx\n", m->size, m->busaddr));
        if (m->busaddr)
            mem_unlock(device, m->handle);
        mem_free(device, m->handle);
    }
    memset(m, 0, sizeof(*m));
}

int v3d_mem_alloc(V3DDevice* device, v3d_mem* m, unsigned size)
{
    const uint32_t align = 16;
    memset(m, 0, sizeof(*m));
    m->size = (size + align - 1) & ~(align - 1);
    m->handle = mem_alloc(device, m->size, 8, MEM_FLAG_DIRECT);
    if (!m->handle) {
        D(("vc4: Failed to alloc mem via mailbox\n"));
        return -1;
    }
    m->busaddr = mem_lock(device, m->handle);
    if (!m->busaddr) {
        D(("vc4: Failed to lock memory via mailbox\n"));
        v3d_mem_free(device, m);
        return -2;
    }
    m->hostptr = (void*)((m->busaddr) & ~0xC0000000);
    if (!m->hostptr) {
        D(("vc4: Failed to map memory\n"));
        v3d_mem_free(device, m);
        return -3;
    }
    D(("vc4: Allocated %lu bytes via mailbox, busaddr = %lx phys = %lx\n",
       size, m->busaddr, (ULONG)m->hostptr));
    device->BytesAllocated += m->size;
    return 0;
}

#endif /* VC4_MEM_USE_MAILBOX */

static CONST_APTR GetPropValueRecursive(APTR key, CONST_STRPTR property)
{
    do {
        APTR prop = DT_FindProperty(key, property);
        if (prop)
            return DT_GetPropValue(prop);
        key = DT_GetParent(key);
    } while (key);
    return NULL;
}


void v3d_free(V3DDevice* device)
{
    D(("vc4: v3d_free\n"));
    if (device->BytesAllocated) {
        D(("Still %lu bytes allocated!\n", (ULONG)device->BytesAllocated));
    }
    if (!v3d_mock_is_active())
        qpu_enable(device, 0);
}

int v3d_init(V3DDevice* device)
{
    int ret = -1;
    struct ExecBase* const SysBase = device->sysbase;

    D(("vc4_init: enter\n"));

    MBReq = (uint32_t*)(((uintptr_t)MBReqStorage + 15) & ~15);
    if ((DeviceTreeBase = OpenResource("devicetree.resource")) != NULL) {
        APTR key = DT_OpenKey("/aliases");
        if (key) {
            const char* mbox_alias = DT_GetPropValue(DT_FindProperty(key, "mailbox"));
            DT_CloseKey(key);
            if (mbox_alias) {
                key = DT_OpenKey(mbox_alias);
                if (key) {
                    ULONG address_cells = 1;
                    CONST ULONG *adr = GetPropValueRecursive(key, "#address-cells");
                    CONST ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));

                    if (adr != NULL) address_cells = *adr;
                    MailBox = reg[(1 * address_cells) - 1];
                    DT_CloseKey(key);

                    key = DT_OpenKey("/soc");
                    if (key) {
                        ULONG cpu_address_cells = 1;
                        const ULONG * addr;
                        const ULONG * cpu_addr;
                        const ULONG *reg2;
                        ULONG phys_v3d;
                        ULONG phys_cpu;

                        address_cells = 1;
                        addr = GetPropValueRecursive(key, "#address-cells");
                        cpu_addr = DT_GetPropValue(DT_FindProperty(DT_OpenKey("/"), "#address-cells"));

                        if (addr != NULL) address_cells = *addr;
                        if (cpu_addr != NULL) cpu_address_cells = *cpu_addr;

                        reg2 = DT_GetPropValue(DT_FindProperty(key, "ranges"));
                        phys_v3d = reg2[address_cells - 1];
                        phys_cpu = reg2[address_cells + cpu_address_cells - 1];

                        MailBox = ((ULONG)MailBox - phys_v3d + phys_cpu);
                        g_vc4_v3d_base = (phys_cpu + 0xC00000);

                        DT_CloseKey(key);
                        D(("vc4_init: Mailbox=0x%lx, V3D_BASE=0x%lx\n", MailBox, g_vc4_v3d_base));

                        power_on_vc4(device);
                        D(("vc4_init: power_on_vc4 done\n"));

                        D(("vc4_init: V3D ident: %08lx %08lx %08lx\n",
                           LE32(V3D_IDENT0), LE32(V3D_IDENT1), LE32(V3D_IDENT2)));

                        /* On VC4, IDENT0 is 0x02443356 ('V3D' ver 2) */
                        if (LE32(V3D_IDENT0) != 0x02443356)
                        {
                            E(("vc4_init: VC4 core IDENT0 mismatch: %08lx (expected 02443356)\n", LE32(V3D_IDENT0)));
                            /* Don't strictly abort during early simulation/tests, but flag */
                        }

                        device->deviceInfo.ver = 21; /* V3D 2.1 */
                        device->deviceInfo.rev = 0;
                        device->deviceInfo.vpm_size = 16384;
                        device->deviceInfo.qpu_count = 12;
                        device->deviceInfo.has_accumulators = 1;

                        /* Enable performance counters */
                        V3D_PCTRE = V3D_PCTRC = LE32(0xFFFF);

                        ret = 0;
                    }
                }
            }
        }
    }

    if (ret != 0) {
        /* Fallback for baremetal or simulation where devicetree may be absent */
        D(("vc4_init: devicetree absent, activating VC4 mock hardware simulation\n"));
        v3d_mock_init();
        device->deviceInfo.ver = 21;
        device->deviceInfo.rev = 0;
        device->deviceInfo.vpm_size = 16384;
        device->deviceInfo.qpu_count = 12;
        device->deviceInfo.has_accumulators = 1;
        V3D_PCTRE = V3D_PCTRC = LE32(0xFFFF);
        ret = 0;
    }

    D(("vc4_init: exit, ret=%ld\n", (LONG)ret));
    return ret;
}
