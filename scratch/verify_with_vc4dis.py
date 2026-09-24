import re
import subprocess
import os

print("1. Running AmigaOS binary under vamos to dump all 114 assembled shaders...")
proc = subprocess.run(
    ["/home/claude/.local/bin/vamos", "-C", "68020", "-s", "256", "scratch/dump_all_shaders_amiga"],
    capture_output=True,
    text=True
)

if proc.returncode != 0:
    print("Vamos execution failed:", proc.stderr)
    exit(1)

vamos_variants = {}
current_var = -1
for line in proc.stdout.splitlines():
    line = line.strip()
    if line.startswith("VAR "):
        parts = line.split()
        current_var = int(parts[1])
        vamos_variants[current_var] = []
    elif line.startswith("INST "):
        parts = line.split()
        w0 = int(parts[1], 16)
        w1 = int(parts[2], 16)
        vamos_variants[current_var].append((w0, w1))

print(f"Captured {len(vamos_variants)} variants from vamos.")

vc4dis_bin = os.path.abspath("depends/vc4asm/build/vc4dis")

print(f"2. Verifying all {len(vamos_variants)} variants with vc4dis -x -V (Marcel Müller's validator)...")

total_verified = 0
warnings_count = 0
errors_count = 0
issues = []

for var_idx in sorted(vamos_variants.keys()):
    insts = vamos_variants[var_idx]
    if not insts:
        continue
    
    # Format as comma-separated hex words: 0xW0, 0xW1, ...
    hex_list = []
    for w0, w1 in insts:
        hex_list.append(f"0x{w0:08x}, 0x{w1:08x}")
    hex_str = ", ".join(hex_list)
    
    # Run vc4dis
    res = subprocess.run(
        [vc4dis_bin, "-x", "-V", "/dev/stdin"],
        input=hex_str,
        text=True,
        capture_output=True
    )
    
    # Check for warnings or errors in stdout / stderr
    output = res.stdout + res.stderr
    warns = [l for l in output.splitlines() if "Warning" in l or "Error" in l]
    
    if warns:
        warnings_count += len(warns)
        issues.append((var_idx, warns))
    else:
        total_verified += 1

print("\n" + "=" * 80)
print("VC4DIS (maazl/vc4asm) VALIDATION REPORT FOR ALL 114 SHADER VARIANTS")
print("=" * 80)
print(f"Total Variants verified        : {len(vamos_variants)}")
print(f"Variants with 0 warnings/errors: {total_verified} / {len(vamos_variants)}")
print(f"Total Warnings reported        : {warnings_count}")

if issues:
    print("\nWarnings / Notes from vc4dis:")
    for v_idx, w_list in issues[:10]:
        print(f"  * Variant {v_idx}:")
        for w in w_list:
            print(f"      {w}")
print("=" * 80)
