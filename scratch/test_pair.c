
#include <stdio.h>
#include <string.h>
#include <ctype.h>
typedef unsigned int v3d_u32;
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


int main() {
    v3d_u32 w0=0, w1=0;
    vc4_assemble_instruction("v8subs r0, -1, r1 ; v8muld r1, r3, r1", &w0, &w1);
    printf("0x%08x, 0x%08x\n", w0, w1);
    return 0;
}
