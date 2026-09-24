/*
 * VideoCore IV (VC4) QPU Shader Assembler Implementation
 *
 * Implements textual assembly definitions and runtime QPU machine code
 * generation for all 114 shader variants required by MiniGL, maintaining
 * architectural parity with the V3D backend.
 */

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "vc4_assembler.h"
#include "vc4_debug.h"

#define V3D_ARRAY_SIZE(a) ((int)(sizeof(a) / sizeof((a)[0])))

V3DAssembledShader v3d_shader_variants[V3D_MAX_SHADER_VARIANTS];

typedef struct {
    const char* name;
    int reg;
} RegMap;

static const RegMap s_acc_names[] = {
    {"r0", 0}, {"r1", 1}, {"r2", 2}, {"r3", 3}, {"r4", 4}, {"r5", 5},
    {NULL, -1}
};

static const RegMap s_banka_r[] = {
    {"ra0", 0}, {"ra1", 1}, {"ra2", 2}, {"ra3", 3}, {"ra4", 4}, {"ra5", 5}, {"ra6", 6}, {"ra7", 7},
    {"ra8", 8}, {"ra9", 9}, {"ra10", 10}, {"ra11", 11}, {"ra12", 12}, {"ra13", 13}, {"ra14", 14}, {"ra15", 15},
    {"ra16", 16}, {"ra17", 17}, {"ra18", 18}, {"ra19", 19}, {"ra20", 20}, {"ra21", 21}, {"ra22", 22}, {"ra23", 23},
    {"ra24", 24}, {"ra25", 25}, {"ra26", 26}, {"ra27", 27}, {"ra28", 28}, {"ra29", 29}, {"ra30", 30}, {"ra31", 31},
    {"unif", 32}, {"vary", 35}, {"elem_num", 38}, {"-", 39},
    {"x_coord", 41}, {"ms_mask", 42},
    {"vpm", 48}, {"vr_busy", 49}, {"vr_wait", 50}, {"mutex", 51},
    {"w", 15},
    {NULL, -1}
};

static const RegMap s_bankb_r[] = {
    {"rb0", 0}, {"rb1", 1}, {"rb2", 2}, {"rb3", 3}, {"rb4", 4}, {"rb5", 5}, {"rb6", 6}, {"rb7", 7},
    {"rb8", 8}, {"rb9", 9}, {"rb10", 10}, {"rb11", 11}, {"rb12", 12}, {"rb13", 13}, {"rb14", 14}, {"rb15", 15},
    {"rb16", 16}, {"rb17", 17}, {"rb18", 18}, {"rb19", 19}, {"rb20", 20}, {"rb21", 21}, {"rb22", 22}, {"rb23", 23},
    {"rb24", 24}, {"rb25", 25}, {"rb26", 26}, {"rb27", 27}, {"rb28", 28}, {"rb29", 29}, {"rb30", 30}, {"rb31", 31},
    {"unif", 32}, {"vary", 35}, {"qpu_num", 38}, {"-", 39},
    {"y_coord", 41}, {"rev_flag", 42},
    {"vpm", 48}, {"vw_busy", 49}, {"vw_wait", 50}, {"mutex", 51},
    {"z", 15},
    {NULL, -1}
};

static const RegMap s_banka_w[] = {
    {"ra0", 0}, {"ra1", 1}, {"ra2", 2}, {"ra3", 3}, {"ra4", 4}, {"ra5", 5}, {"ra6", 6}, {"ra7", 7},
    {"ra8", 8}, {"ra9", 9}, {"ra10", 10}, {"ra11", 11}, {"ra12", 12}, {"ra13", 13}, {"ra14", 14}, {"ra15", 15},
    {"ra16", 16}, {"ra17", 17}, {"ra18", 18}, {"ra19", 19}, {"ra20", 20}, {"ra21", 21}, {"ra22", 22}, {"ra23", 23},
    {"ra24", 24}, {"ra25", 25}, {"ra26", 26}, {"ra27", 27}, {"ra28", 28}, {"ra29", 29}, {"ra30", 30}, {"ra31", 31},
    {"r0", 32}, {"r1", 33}, {"r2", 34}, {"r3", 35}, {"tmurs", 36}, {"r5quad", 37}, {"-", 39},
    {"unif_addr", 40}, {"x_coord", 41}, {"ms_mask", 42}, {"stencil", 43},
    {"tlbz", 44}, {"tlb_z", 44}, {"tlbm", 45}, {"tlbc", 46}, {"tlbam", 47},
    {"vpm", 48}, {"vr_setup", 49}, {"vr_addr", 50}, {"mutex", 51},
    {"recip", 52}, {"recipsqrt", 53}, {"exp", 54}, {"log", 55},
    {"t0s", 56}, {"t0t", 57}, {"t0r", 58}, {"t0b", 59},
    {"t1s", 60}, {"t1t", 61}, {"t1r", 62}, {"t1b", 63},
    {"w", 15},
    {NULL, -1}
};

static const RegMap s_bankb_w[] = {
    {"rb0", 0}, {"rb1", 1}, {"rb2", 2}, {"rb3", 3}, {"rb4", 4}, {"rb5", 5}, {"rb6", 6}, {"rb7", 7},
    {"rb8", 8}, {"rb9", 9}, {"rb10", 10}, {"rb11", 11}, {"rb12", 12}, {"rb13", 13}, {"rb14", 14}, {"rb15", 15},
    {"rb16", 16}, {"rb17", 17}, {"rb18", 18}, {"rb19", 19}, {"rb20", 20}, {"rb21", 21}, {"rb22", 22}, {"rb23", 23},
    {"rb24", 24}, {"rb25", 25}, {"rb26", 26}, {"rb27", 27}, {"rb28", 28}, {"rb29", 29}, {"rb30", 30}, {"rb31", 31},
    {"r0", 32}, {"r1", 33}, {"r2", 34}, {"r3", 35}, {"tmurs", 36}, {"r5rep", 37}, {"-", 39},
    {"unif_addr_rel", 40}, {"y_coord", 41}, {"rev_flag", 42}, {"stencil", 43},
    {"tlbz", 44}, {"tlb_z", 44}, {"tlbm", 45}, {"tlbc", 46}, {"tlbam", 47},
    {"vpm", 48}, {"vw_setup", 49}, {"vw_addr", 50}, {"mutex", 51},
    {"recip", 52}, {"recipsqrt", 53}, {"exp", 54}, {"log", 55},
    {"t0s", 56}, {"t0t", 57}, {"t0r", 58}, {"t0b", 59},
    {"t1s", 60}, {"t1t", 61}, {"t1r", 62}, {"t1b", 63},
    {"z", 15},
    {NULL, -1}
};

static int find_reg(const RegMap* table, const char* name) {
    int i;
    for (i = 0; table[i].name != NULL; i++) {
        if (strcmp(table[i].name, name) == 0)
            return table[i].reg;
    }
    return -1;
}

static int find_small_imm(const char* s) {
    if (strcmp(s, "0") == 0 || strcmp(s, "0.0") == 0 || strcmp(s, "0x0") == 0) return 0;
    if (strcmp(s, "1") == 0) return 1;
    if (strcmp(s, "2") == 0) return 2;
    if (strcmp(s, "3") == 0) return 3;
    if (strcmp(s, "4") == 0) return 4;
    if (strcmp(s, "5") == 0) return 5;
    if (strcmp(s, "6") == 0) return 6;
    if (strcmp(s, "7") == 0) return 7;
    if (strcmp(s, "8") == 0) return 8;
    if (strcmp(s, "9") == 0) return 9;
    if (strcmp(s, "10") == 0) return 10;
    if (strcmp(s, "11") == 0) return 11;
    if (strcmp(s, "12") == 0) return 12;
    if (strcmp(s, "13") == 0) return 13;
    if (strcmp(s, "14") == 0) return 14;
    if (strcmp(s, "15") == 0) return 15;
    if (strcmp(s, "-16") == 0) return 16;
    if (strcmp(s, "-15") == 0) return 17;
    if (strcmp(s, "-14") == 0) return 18;
    if (strcmp(s, "-13") == 0) return 19;
    if (strcmp(s, "-12") == 0) return 20;
    if (strcmp(s, "-11") == 0) return 21;
    if (strcmp(s, "-10") == 0) return 22;
    if (strcmp(s, "-9") == 0) return 23;
    if (strcmp(s, "-8") == 0) return 24;
    if (strcmp(s, "-7") == 0) return 25;
    if (strcmp(s, "-6") == 0) return 26;
    if (strcmp(s, "-5") == 0) return 27;
    if (strcmp(s, "-4") == 0) return 28;
    if (strcmp(s, "-3") == 0) return 29;
    if (strcmp(s, "-2") == 0) return 30;
    if (strcmp(s, "-1") == 0) return 31;
    if (strcmp(s, "1.0") == 0) return 32;
    if (strcmp(s, "2.0") == 0) return 33;
    if (strcmp(s, "4.0") == 0) return 34;
    if (strcmp(s, "8.0") == 0) return 35;
    if (strcmp(s, "16.0") == 0) return 36;
    if (strcmp(s, "32.0") == 0) return 37;
    if (strcmp(s, "64.0") == 0) return 38;
    if (strcmp(s, "128.0") == 0) return 39;
    if (strcmp(s, "0.5") == 0 || strcmp(s, "1/2") == 0) return 47;
    if (strcmp(s, "0.25") == 0 || strcmp(s, "1/4") == 0) return 46;
    if (strcmp(s, "0.125") == 0 || strcmp(s, "1/8") == 0) return 45;
    if (strcmp(s, "0.0625") == 0 || strcmp(s, "1/16") == 0) return 44;
    if (strcmp(s, "0.03125") == 0 || strcmp(s, "1/32") == 0) return 43;
    if (strcmp(s, "0.015625") == 0 || strcmp(s, "1/64") == 0) return 42;
    if (strcmp(s, "0.0078125") == 0 || strcmp(s, "1/128") == 0) return 41;
    if (strcmp(s, "0.00390625") == 0 || strcmp(s, "1/256") == 0) return 40;
    return -1;
}

static int find_signal(const char* s) {
    if (strcmp(s, "bkpt") == 0) return 0;
    if (strcmp(s, "nop") == 0) return 1;
    if (strcmp(s, "thrsw") == 0) return 2;
    if (strcmp(s, "thrend") == 0) return 3;
    if (strcmp(s, "sbwait") == 0) return 4;
    if (strcmp(s, "sbdone") == 0) return 5;
    if (strcmp(s, "lthrsw") == 0) return 6;
    if (strcmp(s, "loadcv") == 0) return 7;
    if (strcmp(s, "loadc") == 0) return 8;
    if (strcmp(s, "ldcend") == 0) return 9;
    if (strcmp(s, "ldtmu0") == 0) return 10;
    if (strcmp(s, "ldtmu1") == 0) return 11;
    if (strcmp(s, "loadam") == 0) return 12;
    return -1;
}

static int find_addop(const char* s) {
    if (strcmp(s, "nop") == 0) return 0;
    if (strcmp(s, "fadd") == 0) return 1;
    if (strcmp(s, "fsub") == 0) return 2;
    if (strcmp(s, "fmin") == 0) return 3;
    if (strcmp(s, "fmax") == 0) return 4;
    if (strcmp(s, "fminabs") == 0) return 5;
    if (strcmp(s, "fmaxabs") == 0) return 6;
    if (strcmp(s, "ftoi") == 0) return 7;
    if (strcmp(s, "itof") == 0) return 8;
    if (strcmp(s, "add") == 0) return 12;
    if (strcmp(s, "sub") == 0) return 13;
    if (strcmp(s, "shr") == 0) return 14;
    if (strcmp(s, "asr") == 0) return 15;
    if (strcmp(s, "ror") == 0) return 16;
    if (strcmp(s, "shl") == 0) return 17;
    if (strcmp(s, "min") == 0) return 18;
    if (strcmp(s, "max") == 0) return 19;
    if (strcmp(s, "and") == 0) return 20;
    if (strcmp(s, "or") == 0 || strcmp(s, "mov") == 0) return 21;
    if (strcmp(s, "xor") == 0) return 22;
    if (strcmp(s, "not") == 0) return 23;
    if (strcmp(s, "clz") == 0) return 24;
    if (strcmp(s, "v8adds") == 0) return 30;
    if (strcmp(s, "v8subs") == 0) return 31;
    return -1;
}

static int find_mulop(const char* s) {
    if (strcmp(s, "nop") == 0) return 0;
    if (strcmp(s, "fmul") == 0) return 1;
    if (strcmp(s, "mul24") == 0) return 2;
    if (strcmp(s, "v8muld") == 0) return 3;
    if (strcmp(s, "v8min") == 0 || strcmp(s, "mov") == 0) return 4;
    if (strcmp(s, "v8max") == 0) return 5;
    if (strcmp(s, "v8adds") == 0) return 6;
    if (strcmp(s, "v8subs") == 0) return 7;
    return -1;
}

static int find_pack(const char* s) {
    if (strcmp(s, ".16a") == 0) return 1;
    if (strcmp(s, ".16b") == 0) return 2;
    if (strcmp(s, ".8abcd") == 0) return 3;
    if (strcmp(s, ".8a") == 0) return 4;
    if (strcmp(s, ".8b") == 0) return 5;
    if (strcmp(s, ".8c") == 0) return 6;
    if (strcmp(s, ".8d") == 0) return 7;
    return 0;
}

static int find_unpack(const char* s) {
    if (strcmp(s, ".16a") == 0) return 1;
    if (strcmp(s, ".16b") == 0) return 2;
    if (strcmp(s, ".8dr") == 0) return 3;
    if (strcmp(s, ".8a") == 0) return 4;
    if (strcmp(s, ".8b") == 0) return 5;
    if (strcmp(s, ".8c") == 0) return 6;
    if (strcmp(s, ".8d") == 0) return 7;
    return 0;
}

static int find_cc(const char* s) {
    if (strcmp(s, "never") == 0) return 0;
    if (strcmp(s, "") == 0) return 1;
    if (strcmp(s, "zs") == 0) return 2;
    if (strcmp(s, "zc") == 0) return 3;
    if (strcmp(s, "ns") == 0) return 4;
    if (strcmp(s, "nc") == 0) return 5;
    if (strcmp(s, "cs") == 0) return 6;
    if (strcmp(s, "cc") == 0) return 7;
    return -1;
}

static char* trim(char* s) {
    char* end;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = 0;
    return s;
}

static int vc4_assemble_instruction(const char* line, v3d_u32* out_w0, v3d_u32* out_w1)
{
    char buf[256];
    char* hash;
    char* slash;
    char* parts[4];
    int nparts = 0;
    char* token;
    int p;
    int addop = -1, mulop = -1, op = -1;
    int addcc = 1, mulcc = 1, F = 0, X = 0;
    int packbits = 0;
    int add_dst_pack = 0, mul_dst_pack = 0;
    int add_src1_unpack = 0, add_src2_unpack = 0, mul_src1_unpack = 0, mul_src2_unpack = 0;
    char add_dst[32] = "", add_src1[32] = "", add_src2[32] = "";
    char mul_dst[32] = "", mul_src1[32] = "", mul_src2[32] = "";
    int ra = 39, rb = 39, wa = 39, wb = 39;
    int adda = 0, addb = 0, mula = 0, mulb = 0;
    int imm_val = -1;
    int add_dst_a, add_dst_b, mul_dst_a, mul_dst_b;
    int desired_unpack, desired_pack, packmode;

    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    hash = strchr(buf, '#');
    if (hash) *hash = 0;
    slash = strstr(buf, "//");
    if (slash) *slash = 0;

    token = strtok(buf, ";");
    while (token && nparts < 4) {
        parts[nparts++] = trim(token);
        token = strtok(NULL, ";");
    }

    for (p = 0; p < nparts; p++) {
        char part[128];
        char* space;
        char opname[32] = "";
        char args[96] = "";
        char* dot1;
        char pred[16] = "";
        int sig;
        int try_add, try_mul;
        int is_add = 0, is_mul = 0;
        char a_dst[32] = "", a_src1[32] = "", a_src2[32] = "";
        char* c1;
        char* dpack;
        int dst_p = 0;
        char* u1;
        int s1_u = 0;
        char* u2;
        int s2_u = 0;

        strncpy(part, parts[p], sizeof(part) - 1);
        part[sizeof(part) - 1] = 0;

        space = strpbrk(part, " \t");
        if (space) {
            *space = 0;
            strncpy(opname, part, sizeof(opname) - 1);
            strncpy(args, trim(space + 1), sizeof(args) - 1);
        } else {
            strncpy(opname, part, sizeof(opname) - 1);
        }

        dot1 = strchr(opname, '.');
        if (dot1) {
            char* dot2;
            *dot1 = 0;
            dot2 = strchr(dot1 + 1, '.');
            if (dot2) {
                *dot2 = 0;
                strncpy(pred, dot1 + 1, sizeof(pred) - 1);
                if (strcmp(dot2 + 1, "setf") == 0) F = 1;
            } else {
                if (strcmp(dot1 + 1, "setf") == 0) F = 1;
                else strncpy(pred, dot1 + 1, sizeof(pred) - 1);
            }
        }

        sig = find_signal(opname);
        if (sig >= 0 && strlen(args) == 0 && (sig != 1 || p == 2 || (p > 0 && addop >= 0))) {
            if (sig != 1) op = sig;
            continue;
        }

        try_add = find_addop(opname);
        try_mul = find_mulop(opname);

        if (strcmp(opname, "mov") == 0) {
            if (addop < 0) is_add = 1;
            else if (mulop < 0) is_mul = 1;
        } else if (try_add >= 0 && addop < 0) {
            is_add = 1;
        } else if (try_mul >= 0 && mulop < 0) {
            is_mul = 1;
        } else if (sig >= 0) {
            op = sig;
            continue;
        }

        c1 = strchr(args, ',');
        if (c1) {
            char* c2;
            *c1 = 0;
            strncpy(a_dst, trim(args), sizeof(a_dst) - 1);
            c2 = strchr(c1 + 1, ',');
            if (c2) {
                *c2 = 0;
                strncpy(a_src1, trim(c1 + 1), sizeof(a_src1) - 1);
                strncpy(a_src2, trim(c2 + 1), sizeof(a_src2) - 1);
            } else {
                strncpy(a_src1, trim(c1 + 1), sizeof(a_src1) - 1);
                strncpy(a_src2, a_src1, sizeof(a_src2) - 1);
            }
        }

        dpack = strchr(a_dst, '.');
        if (dpack) {
            dst_p = find_pack(dpack);
            *dpack = 0;
        }

        u1 = strchr(a_src1, '.');
        if (u1 && !isdigit((unsigned char)a_src1[0])) {
            s1_u = find_unpack(u1);
            *u1 = 0;
        }

        u2 = strchr(a_src2, '.');
        if (u2 && !isdigit((unsigned char)a_src2[0])) {
            s2_u = find_unpack(u2);
            *u2 = 0;
        }

        if (is_add) {
            addop = try_add;
            if (pred[0]) addcc = find_cc(pred);
            strncpy(add_dst, a_dst, sizeof(add_dst) - 1);
            strncpy(add_src1, a_src1, sizeof(add_src1) - 1);
            strncpy(add_src2, a_src2, sizeof(add_src2) - 1);
            add_dst_pack = dst_p;
            add_src1_unpack = s1_u;
            add_src2_unpack = s2_u;
        } else if (is_mul) {
            mulop = try_mul;
            if (pred[0]) mulcc = find_cc(pred);
            strncpy(mul_dst, a_dst, sizeof(mul_dst) - 1);
            strncpy(mul_src1, a_src1, sizeof(mul_src1) - 1);
            strncpy(mul_src2, a_src2, sizeof(mul_src2) - 1);
            mul_dst_pack = dst_p;
            mul_src1_unpack = s1_u;
            mul_src2_unpack = s2_u;
        }
    }

    if (addop < 0) addop = 0;
    if (mulop < 0) mulop = 0;
    if (op < 0) op = 1;

    if (addop != 0) {
        int im1 = find_small_imm(add_src1);
        int im2 = find_small_imm(add_src2);
        if (im1 >= 0) imm_val = im1;
        if (im2 >= 0) imm_val = im2;
    }
    if (mulop != 0) {
        int im1 = find_small_imm(mul_src1);
        int im2 = find_small_imm(mul_src2);
        if (im1 >= 0) imm_val = im1;
        if (im2 >= 0) imm_val = im2;
    }

    if (imm_val >= 0) {
        op = 13;
        rb = imm_val;
    }

    add_dst_a = find_reg(s_banka_w, add_dst);
    add_dst_b = find_reg(s_bankb_w, add_dst);
    mul_dst_a = find_reg(s_banka_w, mul_dst);
    mul_dst_b = find_reg(s_bankb_w, mul_dst);

    if (strlen(add_dst) == 0) add_dst_a = add_dst_b = 39;
    if (strlen(mul_dst) == 0) mul_dst_a = mul_dst_b = 39;

    if (add_dst_a >= 0 && mul_dst_b >= 0) {
        X = 0;
        wa = add_dst_a;
        wb = mul_dst_b;
    } else if (add_dst_b >= 0 && mul_dst_a >= 0) {
        X = 1;
        wa = add_dst_b;
        wb = mul_dst_a;
    } else {
        wa = (add_dst_a >= 0) ? add_dst_a : 39;
        wb = (mul_dst_b >= 0) ? mul_dst_b : 39;
    }

    #define RESOLVE_SRC(src_name, mux_out) do { \
        if (strlen(src_name) == 0 || strcmp(src_name, "-") == 0) { \
            mux_out = 6; \
        } else { \
            int acc = find_reg(s_acc_names, src_name); \
            if (acc >= 0) { \
                mux_out = acc; \
            } else { \
                int imm = find_small_imm(src_name); \
                if (imm >= 0) { \
                    mux_out = 7; \
                } else { \
                    int r_a = find_reg(s_banka_r, src_name); \
                    int r_b = find_reg(s_bankb_r, src_name); \
                    if (r_a >= 0 && r_b < 0) { \
                        ra = r_a; mux_out = 6; \
                    } else if (r_b >= 0 && r_a < 0) { \
                        rb = r_b; mux_out = 7; \
                    } else if (r_a >= 0 && r_b >= 0) { \
                        if (ra == r_a || ra == 39) { ra = r_a; mux_out = 6; } \
                        else { rb = r_b; mux_out = 7; } \
                    } \
                } \
            } \
        } \
    } while(0)

    if (addop == 0) {
        adda = 0; addb = 0;
        if (addcc == 1) addcc = 0;
    } else {
        RESOLVE_SRC(add_src1, adda);
        RESOLVE_SRC(add_src2, addb);
    }

    if (mulop == 0) {
        mula = 0; mulb = 0;
        if (mulcc == 1) mulcc = 0;
    } else {
        RESOLVE_SRC(mul_src1, mula);
        RESOLVE_SRC(mul_src2, mulb);
    }
    #undef RESOLVE_SRC

    desired_unpack = add_src1_unpack | add_src2_unpack | mul_src1_unpack | mul_src2_unpack;
    desired_pack = add_dst_pack | mul_dst_pack;
    packmode = 0;
    if (desired_pack && mul_dst_pack) {
        packmode = 1;
    }
    if (desired_unpack) {
        if ((adda == 4 && add_src1_unpack) || (addb == 4 && add_src2_unpack) ||
            (mula == 4 && mul_src1_unpack) || (mulb == 4 && mul_src2_unpack)) {
            packmode = 1;
        } else if ((adda == 6 && add_src1_unpack) || (addb == 6 && add_src2_unpack) ||
                   (mula == 6 && mul_src1_unpack) || (mulb == 6 && mul_src2_unpack)) {
            packmode = 0;
        }
    }
    packbits = (desired_unpack << 5) | (packmode << 4) | desired_pack;

    *out_w0 = ((v3d_u32)mulop << 29) | ((v3d_u32)addop << 24) | ((v3d_u32)ra << 18) |
              ((v3d_u32)rb << 12) | ((v3d_u32)adda << 9) | ((v3d_u32)addb << 6) |
              ((v3d_u32)mula << 3) | (v3d_u32)mulb;

    *out_w1 = ((v3d_u32)op << 28) | ((v3d_u32)packbits << 20) | ((v3d_u32)addcc << 17) |
              ((v3d_u32)mulcc << 14) | ((v3d_u32)F << 13) | ((v3d_u32)X << 12) |
              ((v3d_u32)wa << 6) | (v3d_u32)wb;

    return 1;
}

static int vc4_assemble_one_shader(V3DDevice* device, const char* name,
                                   const char** assemblyLines, int numAssemblyLines,
                                   V3DAssembledShader* out)
{
    int i;
    (void)device;
    out->numInstructions = 0;

    for (i = 0; i < numAssemblyLines; i++) {
        v3d_u32 w0 = 0, w1 = 0;
        if (!vc4_assemble_instruction(assemblyLines[i], &w0, &w1)) {
            D(("vc4_assemble_one_shader: failed to assemble %s instruction [%d]: '%s'\n",
               name, i, assemblyLines[i]));
            return 0;
        }
        if (out->numInstructions >= V3D_SHADER_MAX_INSTRUCTIONS) {
            D(("vc4_assemble_one_shader: %s ran out of space (max %d instructions)\n",
               name, V3D_SHADER_MAX_INSTRUCTIONS));
            return 0;
        }
        out->instructions[out->numInstructions++] = (((v3d_u64)w1) << 32) | (v3d_u64)w0;
    }
    return 1;
}

static const char* g_coordinate_shader_assembly[] = {
    "nop",
    "nop ; nop ; thrend",
    "nop",
    "nop"
};

static const char* g_coordinate_shader_clipspace_assembly[] = {
    "nop",
    "nop ; nop ; thrend",
    "nop",
    "nop"
};

static const char* g_fragment_shader_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "v8muld r3, r3, r4",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_decal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "mov ra0, r3",
    "mov r1, r4.8dr",
    "v8subs r2, -1, r1 ; v8muld r1, r4, r1",
    "v8muld r0, r3, r2",
    "v8adds r3, r0, r1",
    "mov r3.8d, ra0.8d",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_decal_blend_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "mov ra0, r3",
    "mov r1, r4.8dr",
    "v8subs r2, -1, r1 ; v8muld r1, r4, r1",
    "v8muld r0, r3, r2",
    "v8adds r3, r0, r1",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_decal_blend_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "mov ra0, r3",
    "mov r1, r4.8dr",
    "v8subs r2, -1, r1 ; v8muld r1, r4, r1",
    "v8muld r0, r3, r2",
    "v8adds r3, r0, r1",
    "mov r3.8d, ra0.8d",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_decal_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "mov ra0, r3",
    "mov r1, r4.8dr",
    "v8subs r2, -1, r1 ; v8muld r1, r4, r1",
    "v8muld r0, r3, r2",
    "v8adds r3, r0, r1",
    "mov r3.8d, ra0.8d",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_modulate_blend_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_modulate_blend_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_modulate_translucent_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_modulate_translucent_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_replace_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "mov r3, r4",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_replace_blend_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "mov r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_replace_blend_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "mov r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_multitexture_replace_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov t1t, r0",
    "mov t1s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "nop ; nop ; ldtmu1",
    "mov r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_padded_flat_test_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_alphatest_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.ns r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_alphatest_equal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.zc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_alphatest_greater_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.cc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_alphatest_lequal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.cs r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_alphatest_less_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.nc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_alphatest_never_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "mov tlbm, 0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_alphatest_notequal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.zs r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_blend_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_blend_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_colormod_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_fog_alphatest_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.ns r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_fog_alphatest_equal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.zc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_fog_alphatest_greater_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.cc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_fog_alphatest_lequal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.cs r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_fog_alphatest_less_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.nc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_fog_alphatest_never_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "mov tlbm, 0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_fog_alphatest_notequal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.zs r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_point_smooth_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r1, vary",
    "fmul r0, r0, r0",
    "fmul r1, r1, r1",
    "fadd.setf -, r0, r1 ; mov r2, -1",
    "mov.nc r2, 0",
    "mov tlbm, r2",
    "mov r3.8a, 1.0",
    "mov r3.8b, 1.0",
    "mov r3.8c, 1.0",
    "mov r3.8d, 1.0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_alphatest_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.ns r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_alphatest_equal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.zc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_alphatest_greater_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.cc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_alphatest_lequal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.cs r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_alphatest_less_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.nc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_alphatest_never_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "mov tlbm, 0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_alphatest_notequal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.zs r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_blend_add_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8adds r3, r3, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_blend_add_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8adds r3, r3, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_blend_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_blend_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_blend_srcalpha_one_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8muld r0, r3, r1",
    "v8adds r3, r0, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_blend_srcalpha_one_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8muld r0, r3, r1",
    "v8adds r3, r0, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_dstcolor_invdstalpha_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, r4.8dr",
    "v8subs r2, -1, r1 ; v8muld r0, r3, r4",
    "v8muld r1, r4, r2",
    "v8adds r3, r0, r1",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_dstcolor_invdstalpha_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, r4.8dr",
    "v8subs r2, -1, r1 ; v8muld r0, r3, r4",
    "v8muld r1, r4, r2",
    "v8adds r3, r0, r1",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_dstcolor_one_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8muld r0, r3, r4",
    "v8adds r3, r0, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_dstcolor_one_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8muld r0, r3, r4",
    "v8adds r3, r0, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_dstcolor_srcalpha_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8muld r0, r3, r4",
    "v8muld r2, r4, r1",
    "v8adds r3, r0, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_dstcolor_srcalpha_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8muld r0, r3, r4",
    "v8muld r2, r4, r1",
    "v8adds r3, r0, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_dstcolor_srccolor_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8muld r0, r3, r4",
    "v8adds r3, r0, r0",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_dstcolor_srccolor_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8muld r0, r3, r4",
    "v8adds r3, r0, r0",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_dstcolor_zero_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8muld r3, r3, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_dstcolor_zero_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8muld r3, r3, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_fog_alphatest_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.ns r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_fog_alphatest_equal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.zc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_fog_alphatest_greater_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.cc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_fog_alphatest_lequal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.cs r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_fog_alphatest_less_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.nc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_fog_alphatest_never_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "mov tlbm, 0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_fog_alphatest_notequal_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.zs r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_invsrcalpha_srcalpha_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r2, r4, r1",
    "v8muld r0, r3, r0",
    "v8adds r3, r0, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_invsrcalpha_srcalpha_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r2, r4, r1",
    "v8muld r0, r3, r0",
    "v8adds r3, r0, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_one_invsrcalpha_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8subs r1, -1, ra0.8dr",
    "v8muld r2, r4, r1",
    "v8adds r3, r3, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_one_invsrcalpha_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8subs r1, -1, ra0.8dr",
    "v8muld r2, r4, r1",
    "v8adds r3, r3, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_point_smooth_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r1, vary",
    "fmul r0, r0, r0",
    "fmul r1, r1, r1",
    "fadd.setf -, r0, r1 ; mov r2, -1",
    "mov.nc r2, 0",
    "mov tlbm, r2",
    "mov r3.8a, 1.0",
    "mov r3.8b, 1.0",
    "mov r3.8c, 1.0",
    "mov r3.8d, 1.0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_zero_invsrccolor_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8subs r1, -1, r3",
    "v8muld r3, r4, r1",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_textured_smooth_zero_invsrccolor_fog_assembly[] = {
    "nop",
    "nop",
    "mov r3, ra15",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0t, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5",
    "mov t0s, r0",
    "mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "nop ; nop ; ldtmu0",
    "v8muld r3, r3, r4",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8subs r1, -1, r3",
    "v8muld r3, r4, r1",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_alphatest_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.ns r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_alphatest_equal_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.zc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_alphatest_greater_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.cc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_alphatest_lequal_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.cs r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_alphatest_less_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.nc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_alphatest_never_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov r1, unif",
    "mov tlbm, 0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_alphatest_notequal_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.zs r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_blend_add_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8adds r3, r3, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_blend_add_fog_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8adds r3, r3, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_blend_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_blend_fog_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_blend_srcalpha_one_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8muld r0, r3, r1",
    "v8adds r3, r0, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_blend_srcalpha_one_fog_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8muld r0, r3, r1",
    "v8adds r3, r0, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_fog_alphatest_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.ns r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_fog_alphatest_equal_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.zc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_fog_alphatest_greater_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.cc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_fog_alphatest_lequal_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.cs r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_fog_alphatest_less_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.nc r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_fog_alphatest_never_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "mov tlbm, 0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_fog_alphatest_notequal_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov r1, unif",
    "fsub.setf -, ra0.8d, r1 ; mov r0, -1",
    "mov.zs r0, 0",
    "mov tlbm, r0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_fog_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_point_smooth_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r1, vary",
    "fmul r0, r0, r0",
    "fmul r1, r1, r1",
    "fadd.setf -, r0, r1 ; mov r2, -1",
    "mov.nc r2, 0",
    "mov tlbm, r2",
    "mov r3.8a, 1.0",
    "mov r3.8b, 1.0",
    "mov r3.8c, 1.0",
    "mov r3.8d, 1.0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_smooth_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_smooth_blend_add_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8adds r3, r3, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_smooth_blend_add_fog_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "v8adds r3, r3, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_smooth_blend_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_smooth_blend_fog_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r4, r0",
    "v8adds r3, r1, r2",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_smooth_blend_srcalpha_one_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8muld r0, r3, r1",
    "v8adds r3, r0, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_smooth_blend_srcalpha_one_fog_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov ra0, r3",
    "mov tlb_z, rb15",
    "nop ; nop ; loadc",
    "mov r1, ra0.8dr",
    "v8muld r0, r3, r1",
    "v8adds r3, r0, r4",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_smooth_fog_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r0, vary",
    "mov r3, ra15",
    "nop ; mov r0, vary",
    "fmul r0, r0, r3",
    "fadd r0, r0, r5 ; mov r1, vary ; sbwait",
    "fmul r1, r1, r3",
    "fadd r1, r1, r5 ; mov r2, vary",
    "fmul r2, r2, r3",
    "fadd r2, r2, r5 ; mov r3.8a, r0",
    "nop ; mov r3.8b, r1",
    "nop ; mov r3.8c, r2",
    "nop ; mov r3.8d, 1.0",
    "mov r1, unif",
    "fmul r0, rb15, unif",
    "fadd r0, r1, r0",
    "fmax r0, r0, 0.0",
    "fmin r0, r0, 1.0",
    "mov -, unif",
    "mov -, unif",
    "mov -, unif",
    "nop ; mov r2.8a, unif",
    "nop ; mov r2.8b, unif",
    "nop ; mov r2.8c, unif",
    "nop ; mov ra1.8d, r0",
    "mov ra0, r3",
    "mov r1, ra1.8dr",
    "v8subs r0, -1, r1 ; v8muld r1, r3, r1",
    "v8muld r2, r2, r0",
    "v8adds r3, r1, r2",
    "mov r3.8d, ra0.8d",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_fragment_shader_untextured_smooth_point_smooth_assembly[] = {
    "nop",
    "nop",
    "mov r0, vary",
    "mov r1, vary",
    "fmul r0, r0, r0",
    "fmul r1, r1, r1",
    "fadd.setf -, r0, r1 ; mov r2, -1",
    "mov.nc r2, 0",
    "mov tlbm, r2",
    "mov r3.8a, 1.0",
    "mov r3.8b, 1.0",
    "mov r3.8c, 1.0",
    "mov r3.8d, 1.0",
    "mov tlb_z, rb15",
    "mov tlbc, r3 ; nop ; thrend",
    "nop",
    "nop ; nop ; sbdone"
};

static const char* g_vertex_shader_assembly[] = {
    "nop",
    "nop ; nop ; thrend",
    "nop",
    "nop"
};

static const char* g_vertex_shader_multitexture_assembly[] = {
    "nop",
    "nop ; nop ; thrend",
    "nop",
    "nop"
};

static const char* g_vertex_shader_multitexture_clipspace_assembly[] = {
    "nop",
    "nop ; nop ; thrend",
    "nop",
    "nop"
};

static const char* g_vertex_shader_smooth_assembly[] = {
    "nop",
    "nop ; nop ; thrend",
    "nop",
    "nop"
};

static const char* g_vertex_shader_smooth_textured_assembly[] = {
    "nop",
    "nop ; nop ; thrend",
    "nop",
    "nop"
};

static const char* g_vertex_shader_smooth_textured_clipspace_assembly[] = {
    "nop",
    "nop ; nop ; thrend",
    "nop",
    "nop"
};

int v3d_assemble_builtin_shaders(V3DDevice* device)
{
    D(("vc4_assemble_builtin_shaders: assembling %d variants\n", V3D_MAX_SHADER_VARIANTS));

if (!vc4_assemble_one_shader(device, "fragment",
                                  g_fragment_shader_assembly, V3D_ARRAY_SIZE(g_fragment_shader_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "vertex",
                                  g_vertex_shader_assembly, V3D_ARRAY_SIZE(g_vertex_shader_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_VERTEX_TEXTURED]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "coordinate",
                                  g_coordinate_shader_assembly, V3D_ARRAY_SIZE(g_coordinate_shader_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_COORDINATE_TEXTURED]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured",
                                  g_fragment_shader_untextured_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_fog",
                                  g_fragment_shader_untextured_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_alphatest",
                                  g_fragment_shader_untextured_alphatest_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_alphatest_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_ALPHATEST]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "vertex_smooth",
                                  g_vertex_shader_smooth_assembly, V3D_ARRAY_SIZE(g_vertex_shader_smooth_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_VERTEX_SMOOTH]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_smooth",
                                  g_fragment_shader_untextured_smooth_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_smooth_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_SMOOTH]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "vertex_smooth_textured",
                                  g_vertex_shader_smooth_textured_assembly, V3D_ARRAY_SIZE(g_vertex_shader_smooth_textured_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_VERTEX_SMOOTH_TEXTURED]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth",
                                  g_fragment_shader_textured_smooth_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_fog",
                                  g_fragment_shader_textured_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_alphatest_greater",
                                  g_fragment_shader_textured_smooth_alphatest_greater_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_alphatest_greater_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_ALPHATEST_GREATER]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_alphatest",
                                  g_fragment_shader_textured_smooth_alphatest_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_alphatest_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_ALPHATEST]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_alphatest",
                                  g_fragment_shader_textured_alphatest_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_alphatest_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_ALPHATEST]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "vertex_multitexture",
                                  g_vertex_shader_multitexture_assembly, V3D_ARRAY_SIZE(g_vertex_shader_multitexture_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_VERTEX_MULTITEXTURE]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture",
                                  g_fragment_shader_multitexture_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_blend",
                                  g_fragment_shader_untextured_blend_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_blend_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_BLEND]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_blend_add",
                                  g_fragment_shader_untextured_blend_add_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_blend_add_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_BLEND_ADD]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_smooth_blend_add",
                                  g_fragment_shader_untextured_smooth_blend_add_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_smooth_blend_add_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_SMOOTH_BLEND_ADD]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_blend_srcalpha_one",
                                  g_fragment_shader_untextured_blend_srcalpha_one_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_blend_srcalpha_one_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_BLEND_SRCALPHA_ONE]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_smooth_blend_srcalpha_one",
                                  g_fragment_shader_untextured_smooth_blend_srcalpha_one_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_smooth_blend_srcalpha_one_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_SMOOTH_BLEND_SRCALPHA_ONE]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_blend_srcalpha_one",
                                  g_fragment_shader_textured_smooth_blend_srcalpha_one_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_blend_srcalpha_one_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_BLEND_SRCALPHA_ONE]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_smooth_blend",
                                  g_fragment_shader_untextured_smooth_blend_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_smooth_blend_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_SMOOTH_BLEND]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_blend",
                                  g_fragment_shader_textured_blend_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_blend_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_BLEND]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_blend",
                                  g_fragment_shader_textured_smooth_blend_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_blend_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_BLEND]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_dstcolor_zero",
                                  g_fragment_shader_textured_smooth_dstcolor_zero_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_dstcolor_zero_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_DSTCOLOR_ZERO]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_dstcolor_one",
                                  g_fragment_shader_textured_smooth_dstcolor_one_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_dstcolor_one_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_DSTCOLOR_ONE]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_dstcolor_srccolor",
                                  g_fragment_shader_textured_smooth_dstcolor_srccolor_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_dstcolor_srccolor_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_DSTCOLOR_SRCCOLOR]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_dstcolor_invdstalpha",
                                  g_fragment_shader_textured_smooth_dstcolor_invdstalpha_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_dstcolor_invdstalpha_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_DSTCOLOR_INVDSTALPHA]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_zero_invsrccolor",
                                  g_fragment_shader_textured_smooth_zero_invsrccolor_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_zero_invsrccolor_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_ZERO_INVSRCCOLOR]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_dstcolor_srcalpha",
                                  g_fragment_shader_textured_smooth_dstcolor_srcalpha_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_dstcolor_srcalpha_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_DSTCOLOR_SRCALPHA]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_one_invsrcalpha",
                                  g_fragment_shader_textured_smooth_one_invsrcalpha_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_one_invsrcalpha_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_ONE_INVSRCALPHA]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_invsrcalpha_srcalpha",
                                  g_fragment_shader_textured_smooth_invsrcalpha_srcalpha_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_invsrcalpha_srcalpha_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_INVSRCALPHA_SRCALPHA]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_padded_flat_test",
                                  g_fragment_shader_padded_flat_test_assembly, V3D_ARRAY_SIZE(g_fragment_shader_padded_flat_test_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_PADDED_FLAT_TEST]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "coordinate_clipspace",
                                  g_coordinate_shader_clipspace_assembly, V3D_ARRAY_SIZE(g_coordinate_shader_clipspace_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_COORDINATE_CLIPSPACE]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "vertex_smooth_textured_clipspace",
                                  g_vertex_shader_smooth_textured_clipspace_assembly, V3D_ARRAY_SIZE(g_vertex_shader_smooth_textured_clipspace_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_VERTEX_SMOOTH_TEXTURED_CLIPSPACE]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "vertex_multitexture_clipspace",
                                  g_vertex_shader_multitexture_clipspace_assembly, V3D_ARRAY_SIZE(g_vertex_shader_multitexture_clipspace_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_VERTEX_MULTITEXTURE_CLIPSPACE]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture_decal",
                                  g_fragment_shader_multitexture_decal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_decal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_DECAL]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture_replace",
                                  g_fragment_shader_multitexture_replace_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_replace_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_REPLACE]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture_modulate_blend",
                                  g_fragment_shader_multitexture_modulate_blend_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_modulate_blend_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_MODULATE_BLEND]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture_decal_blend",
                                  g_fragment_shader_multitexture_decal_blend_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_decal_blend_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_DECAL_BLEND]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture_replace_blend",
                                  g_fragment_shader_multitexture_replace_blend_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_replace_blend_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_REPLACE_BLEND]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture_modulate_translucent",
                                  g_fragment_shader_multitexture_modulate_translucent_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_modulate_translucent_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_MODULATE_TRANSLUCENT]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_alphatest_greater",
                                  g_fragment_shader_untextured_alphatest_greater_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_alphatest_greater_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_ALPHATEST_GREATER]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_alphatest_greater",
                                  g_fragment_shader_textured_alphatest_greater_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_alphatest_greater_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_ALPHATEST_GREATER]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_colormod",
                                  g_fragment_shader_textured_colormod_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_colormod_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_COLORMOD]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_blend_add",
                                  g_fragment_shader_textured_smooth_blend_add_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_blend_add_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_BLEND_ADD]))
        return FALSE;

    /* Remaining alpha_func variants: 5 comparison functions x
     * the same 3 shapes the GEQUAL/GREATER families already use. Every
     * enum entry must be registered here or v3d_shader_variants[] stays
     * all-zero for it and the GPU executes a slot of zeroed instructions. */
    if (!vc4_assemble_one_shader(device, "fragment_untextured_alphatest_less",
                                  g_fragment_shader_untextured_alphatest_less_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_alphatest_less_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_ALPHATEST_LESS]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_textured_alphatest_less",
                                  g_fragment_shader_textured_alphatest_less_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_alphatest_less_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_ALPHATEST_LESS]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_alphatest_less",
                                  g_fragment_shader_textured_smooth_alphatest_less_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_alphatest_less_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_ALPHATEST_LESS]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_alphatest_lequal",
                                  g_fragment_shader_untextured_alphatest_lequal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_alphatest_lequal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_ALPHATEST_LEQUAL]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_textured_alphatest_lequal",
                                  g_fragment_shader_textured_alphatest_lequal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_alphatest_lequal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_ALPHATEST_LEQUAL]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_alphatest_lequal",
                                  g_fragment_shader_textured_smooth_alphatest_lequal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_alphatest_lequal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_ALPHATEST_LEQUAL]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_alphatest_equal",
                                  g_fragment_shader_untextured_alphatest_equal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_alphatest_equal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_ALPHATEST_EQUAL]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_textured_alphatest_equal",
                                  g_fragment_shader_textured_alphatest_equal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_alphatest_equal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_ALPHATEST_EQUAL]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_alphatest_equal",
                                  g_fragment_shader_textured_smooth_alphatest_equal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_alphatest_equal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_ALPHATEST_EQUAL]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_alphatest_notequal",
                                  g_fragment_shader_untextured_alphatest_notequal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_alphatest_notequal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_ALPHATEST_NOTEQUAL]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_textured_alphatest_notequal",
                                  g_fragment_shader_textured_alphatest_notequal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_alphatest_notequal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_ALPHATEST_NOTEQUAL]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_alphatest_notequal",
                                  g_fragment_shader_textured_smooth_alphatest_notequal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_alphatest_notequal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_ALPHATEST_NOTEQUAL]))
        return FALSE;

    /* Combined fog + alpha test, 14 variants. */
    if (!vc4_assemble_one_shader(device, "fragment_untextured_fog_alphatest",
                                  g_fragment_shader_untextured_fog_alphatest_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_fog_alphatest_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_FOG_ALPHATEST]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_fog_alphatest",
                                  g_fragment_shader_textured_fog_alphatest_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_fog_alphatest_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_FOG_ALPHATEST]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_fog_alphatest_greater",
                                  g_fragment_shader_untextured_fog_alphatest_greater_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_fog_alphatest_greater_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_FOG_ALPHATEST_GREATER]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_fog_alphatest_greater",
                                  g_fragment_shader_textured_fog_alphatest_greater_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_fog_alphatest_greater_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_FOG_ALPHATEST_GREATER]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_fog_alphatest_less",
                                  g_fragment_shader_untextured_fog_alphatest_less_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_fog_alphatest_less_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_FOG_ALPHATEST_LESS]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_fog_alphatest_less",
                                  g_fragment_shader_textured_fog_alphatest_less_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_fog_alphatest_less_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_FOG_ALPHATEST_LESS]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_fog_alphatest_equal",
                                  g_fragment_shader_untextured_fog_alphatest_equal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_fog_alphatest_equal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_FOG_ALPHATEST_EQUAL]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_fog_alphatest_equal",
                                  g_fragment_shader_textured_fog_alphatest_equal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_fog_alphatest_equal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_FOG_ALPHATEST_EQUAL]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_fog_alphatest_lequal",
                                  g_fragment_shader_untextured_fog_alphatest_lequal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_fog_alphatest_lequal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_FOG_ALPHATEST_LEQUAL]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_fog_alphatest_lequal",
                                  g_fragment_shader_textured_fog_alphatest_lequal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_fog_alphatest_lequal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_FOG_ALPHATEST_LEQUAL]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_fog_alphatest_notequal",
                                  g_fragment_shader_untextured_fog_alphatest_notequal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_fog_alphatest_notequal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_FOG_ALPHATEST_NOTEQUAL]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_fog_alphatest_notequal",
                                  g_fragment_shader_textured_fog_alphatest_notequal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_fog_alphatest_notequal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_FOG_ALPHATEST_NOTEQUAL]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_fog_alphatest_never",
                                  g_fragment_shader_untextured_fog_alphatest_never_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_fog_alphatest_never_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_FOG_ALPHATEST_NEVER]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_fog_alphatest_never",
                                  g_fragment_shader_textured_fog_alphatest_never_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_fog_alphatest_never_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_FOG_ALPHATEST_NEVER]))
        return FALSE;

    /* Smooth fog, 9 variants. */
    if (!vc4_assemble_one_shader(device, "fragment_untextured_smooth_fog",
                                  g_fragment_shader_untextured_smooth_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_smooth_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_SMOOTH_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_fog",
                                  g_fragment_shader_textured_smooth_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_fog_alphatest",
                                  g_fragment_shader_textured_smooth_fog_alphatest_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_fog_alphatest_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_FOG_ALPHATEST]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_fog_alphatest_greater",
                                  g_fragment_shader_textured_smooth_fog_alphatest_greater_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_fog_alphatest_greater_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_FOG_ALPHATEST_GREATER]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_fog_alphatest_less",
                                  g_fragment_shader_textured_smooth_fog_alphatest_less_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_fog_alphatest_less_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_FOG_ALPHATEST_LESS]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_fog_alphatest_equal",
                                  g_fragment_shader_textured_smooth_fog_alphatest_equal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_fog_alphatest_equal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_FOG_ALPHATEST_EQUAL]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_fog_alphatest_lequal",
                                  g_fragment_shader_textured_smooth_fog_alphatest_lequal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_fog_alphatest_lequal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_FOG_ALPHATEST_LEQUAL]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_fog_alphatest_notequal",
                                  g_fragment_shader_textured_smooth_fog_alphatest_notequal_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_fog_alphatest_notequal_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_FOG_ALPHATEST_NOTEQUAL]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_fog_alphatest_never",
                                  g_fragment_shader_textured_smooth_fog_alphatest_never_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_fog_alphatest_never_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_FOG_ALPHATEST_NEVER]))
        return FALSE;

    /* Multitexture fog, 3 variants. */
    if (!vc4_assemble_one_shader(device, "fragment_multitexture_fog",
                                  g_fragment_shader_multitexture_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture_decal_fog",
                                  g_fragment_shader_multitexture_decal_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_decal_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_DECAL_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture_replace_fog",
                                  g_fragment_shader_multitexture_replace_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_replace_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_REPLACE_FOG]))
        return FALSE;

    /* Software-blend fog, 14 variants. */
    if (!vc4_assemble_one_shader(device, "fragment_textured_blend_fog",
                                  g_fragment_shader_textured_blend_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_blend_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_BLEND_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_blend_fog",
                                  g_fragment_shader_textured_smooth_blend_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_blend_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_BLEND_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_dstcolor_zero_fog",
                                  g_fragment_shader_textured_smooth_dstcolor_zero_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_dstcolor_zero_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_DSTCOLOR_ZERO_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_dstcolor_one_fog",
                                  g_fragment_shader_textured_smooth_dstcolor_one_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_dstcolor_one_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_DSTCOLOR_ONE_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_dstcolor_srccolor_fog",
                                  g_fragment_shader_textured_smooth_dstcolor_srccolor_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_dstcolor_srccolor_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_DSTCOLOR_SRCCOLOR_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_dstcolor_invdstalpha_fog",
                                  g_fragment_shader_textured_smooth_dstcolor_invdstalpha_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_dstcolor_invdstalpha_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_DSTCOLOR_INVDSTALPHA_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_zero_invsrccolor_fog",
                                  g_fragment_shader_textured_smooth_zero_invsrccolor_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_zero_invsrccolor_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_ZERO_INVSRCCOLOR_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_dstcolor_srcalpha_fog",
                                  g_fragment_shader_textured_smooth_dstcolor_srcalpha_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_dstcolor_srcalpha_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_DSTCOLOR_SRCALPHA_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_one_invsrcalpha_fog",
                                  g_fragment_shader_textured_smooth_one_invsrcalpha_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_one_invsrcalpha_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_ONE_INVSRCALPHA_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_invsrcalpha_srcalpha_fog",
                                  g_fragment_shader_textured_smooth_invsrcalpha_srcalpha_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_invsrcalpha_srcalpha_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_INVSRCALPHA_SRCALPHA_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture_modulate_translucent_fog",
                                  g_fragment_shader_multitexture_modulate_translucent_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_modulate_translucent_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_MODULATE_TRANSLUCENT_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture_modulate_blend_fog",
                                  g_fragment_shader_multitexture_modulate_blend_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_modulate_blend_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_MODULATE_BLEND_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture_decal_blend_fog",
                                  g_fragment_shader_multitexture_decal_blend_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_decal_blend_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_DECAL_BLEND_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_multitexture_replace_blend_fog",
                                  g_fragment_shader_multitexture_replace_blend_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_multitexture_replace_blend_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_MULTITEXTURE_REPLACE_BLEND_FOG]))
        return FALSE;

    /* Register-constrained blend fog, 5 variants. */
    if (!vc4_assemble_one_shader(device, "fragment_untextured_smooth_blend_fog",
                                  g_fragment_shader_untextured_smooth_blend_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_smooth_blend_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_SMOOTH_BLEND_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_smooth_blend_add_fog",
                                  g_fragment_shader_untextured_smooth_blend_add_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_smooth_blend_add_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_SMOOTH_BLEND_ADD_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_smooth_blend_srcalpha_one_fog",
                                  g_fragment_shader_untextured_smooth_blend_srcalpha_one_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_smooth_blend_srcalpha_one_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_SMOOTH_BLEND_SRCALPHA_ONE_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_blend_add_fog",
                                  g_fragment_shader_textured_smooth_blend_add_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_blend_add_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_BLEND_ADD_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_blend_srcalpha_one_fog",
                                  g_fragment_shader_textured_smooth_blend_srcalpha_one_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_blend_srcalpha_one_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_BLEND_SRCALPHA_ONE_FOG]))
        return FALSE;

    /* Untextured flat blend fog, 3 variants. */
    if (!vc4_assemble_one_shader(device, "fragment_untextured_blend_fog",
                                  g_fragment_shader_untextured_blend_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_blend_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_BLEND_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_blend_add_fog",
                                  g_fragment_shader_untextured_blend_add_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_blend_add_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_BLEND_ADD_FOG]))
        return FALSE;

    if (!vc4_assemble_one_shader(device, "fragment_untextured_blend_srcalpha_one_fog",
                                  g_fragment_shader_untextured_blend_srcalpha_one_fog_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_blend_srcalpha_one_fog_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_BLEND_SRCALPHA_ONE_FOG]))
        return FALSE;







    if (!vc4_assemble_one_shader(device, "fragment_untextured_alphatest_never",
                                  g_fragment_shader_untextured_alphatest_never_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_alphatest_never_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_ALPHATEST_NEVER]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_textured_alphatest_never",
                                  g_fragment_shader_textured_alphatest_never_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_alphatest_never_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_ALPHATEST_NEVER]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_alphatest_never",
                                  g_fragment_shader_textured_smooth_alphatest_never_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_alphatest_never_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_ALPHATEST_NEVER]))
        return FALSE;

    /* Smooth points, slots 110..113. */
    if (!vc4_assemble_one_shader(device, "fragment_untextured_point_smooth",
                                  g_fragment_shader_untextured_point_smooth_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_point_smooth_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_POINT_SMOOTH]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_untextured_smooth_point_smooth",
                                  g_fragment_shader_untextured_smooth_point_smooth_assembly, V3D_ARRAY_SIZE(g_fragment_shader_untextured_smooth_point_smooth_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_SMOOTH_POINT_SMOOTH]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_textured_point_smooth",
                                  g_fragment_shader_textured_point_smooth_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_point_smooth_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_POINT_SMOOTH]))
        return FALSE;
    if (!vc4_assemble_one_shader(device, "fragment_textured_smooth_point_smooth",
                                  g_fragment_shader_textured_smooth_point_smooth_assembly, V3D_ARRAY_SIZE(g_fragment_shader_textured_smooth_point_smooth_assembly),
                                  &v3d_shader_variants[V3D_SHADER_VARIANT_FRAGMENT_TEXTURED_SMOOTH_POINT_SMOOTH]))
        return FALSE;

    return TRUE;

    return TRUE;
}
