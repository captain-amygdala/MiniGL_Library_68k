/*
 * VideoCore IV (VC4) Debug output
 */

#ifdef DEBUG

#include <stdarg.h>
#include <exec/execbase.h>
#include <proto/exec.h>

#include "lvocall_compat.h"

MGLV3D_LP1(APTR, __DRawPutChar, 516, UBYTE, d0)
#define DRawPutChar(MyChar) __DRawPutChar(SysBase, (MyChar))

void DPutChProc(MGLV3D_REG(d0, UBYTE mychar), MGLV3D_REG(a3, APTR PutChData))
{
    /* Emu68 synchronous serial debug: writing a byte to 0xdeadbeef
     * triggers Emu68 kprintf("%c", value) at 20Mbit to the FTDI opto interface */
    *(volatile UBYTE *)0xdeadbeef = mychar;
    (void)PutChData;
}

void kprintf(STRPTR format, ...)
{
    if (format)
    {
        struct ExecBase* SysBase = *(struct ExecBase **)4L;
        va_list args;
        va_start(args, format);
        RawDoFmt(format, (APTR)args, (void (*)())&DPutChProc, (APTR)SysBase);
        va_end(args);
    }
    return;
}

#endif
