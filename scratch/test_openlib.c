#include <stdio.h>
#include <proto/exec.h>

int main(void)
{
    struct Library *gbase = OpenLibrary("graphics.library", 0);
    struct Library *ibase = OpenLibrary("intuition.library", 0);
    struct Library *ubase = OpenLibrary("utility.library", 0);
    struct Library *cbase = OpenLibrary("cybergraphics.library", 0);

    printf("graphics: %p\n", (void*)gbase);
    printf("intuition: %p\n", (void*)ibase);
    printf("utility: %p\n", (void*)ubase);
    printf("cybergraphics: %p\n", (void*)cbase);

    if (gbase) CloseLibrary(gbase);
    if (ibase) CloseLibrary(ibase);
    if (ubase) CloseLibrary(ubase);
    if (cbase) CloseLibrary(cbase);

    return 0;
}
