#include <stdio.h>
#include <string.h>
#include <proto/exec.h>
#include "v3d_device.h"
#include "v3d_shader_assembler.h"

extern V3DAssembledShader v3d_shader_variants[];
extern int v3d_assemble_builtin_shaders(V3DDevice* device);

int main() {
    V3DDevice dev;
    memset(&dev, 0, sizeof(dev));
    if (!v3d_assemble_builtin_shaders(&dev)) {
        printf("ERROR: v3d_assemble_builtin_shaders failed\n");
        return 1;
    }
    for (int i = 0; i < V3D_MAX_SHADER_VARIANTS; i++) {
        int n = v3d_shader_variants[i].numInstructions;
        printf("VAR %d %d\n", i, n);
        for (int j = 0; j < n; j++) {
            v3d_qpu_instruction inst = v3d_shader_variants[i].instructions[j];
            uint32_t w0 = (uint32_t)(inst & 0xFFFFFFFFULL);
            uint32_t w1 = (uint32_t)((inst >> 32) & 0xFFFFFFFFULL);
            printf("INST %08lx %08lx\n", (unsigned long)w0, (unsigned long)w1);
        }
    }
    return 0;
}
