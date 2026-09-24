import re
import subprocess
import json
import os

print("1. Parsing enum values from v3d_shader_assembler.h...")
with open("backend/vc4/include/v3d_shader_assembler.h") as f:
    hdr = f.read()

m = re.search(r'enum\s*\{([^}]+)\};', hdr)
enum_body = m.group(1)
enum_map = {}
val = 0
for line in enum_body.splitlines():
    line = re.sub(r'/\*.*?\*/', '', line).strip()
    if not line or line.startswith('//'):
        continue
    parts = line.split(',')
    for part in parts:
        part = part.strip()
        if not part:
            continue
        if '=' in part:
            k, v = part.split('=')
            val = int(v.strip())
            enum_map[k.strip()] = val
        else:
            enum_map[part.strip()] = val
        val += 1

print(f"Parsed {len(enum_map)} enums.")

print("2. Running AmigaOS binary under vamos to dump all 114 assembled shaders...")
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

# 3. Extract assembly arrays from vc4_assembler.c
with open("backend/vc4/hw/vc4_assembler.c", "r") as f:
    vc4_c = f.read()

array_pat = re.compile(r'static const char\*\s+(g_[a-zA-Z0-9_]+_assembly)\[\]\s*=\s*\{([^}]+)\};', re.MULTILINE)
arrays = {}
for m in array_pat.finditer(vc4_c):
    arr_name = m.group(1)
    body = m.group(2)
    lines = re.findall(r'"([^"]+)"', body)
    arrays[arr_name] = lines

print(f"Extracted {len(arrays)} assembly arrays from vc4_assembler.c.")

call_pat = re.compile(r'vc4_assemble_one_shader\s*\(\s*device\s*,\s*"([^"]+)"\s*,\s*(g_[a-zA-Z0-9_]+_assembly)\s*,\s*[^,]+\s*,\s*&v3d_shader_variants\[([^\]]+)\]')
variant_calls = call_pat.findall(vc4_c)
print(f"Found {len(variant_calls)} shader registration calls.")

# Now prepare JS script to assemble all arrays with qpuasm.js
js_tests = []
for name, arr_name, var_id in variant_calls:
    lines = arrays[arr_name]
    cleaned_lines = [l.replace("tlb_z", "tlbz") for l in lines]
    enum_idx = enum_map[var_id]
    js_tests.append({
        "var_name": name,
        "arr_name": arr_name,
        "var_id": var_id,
        "enum_idx": enum_idx,
        "lines": cleaned_lines
    })

with open("scratch/js_input.json", "w") as f:
    json.dump(js_tests, f)

qpuasm_abs_path = os.path.abspath("depends/RaspberryPI/RPI2/RPI2_TRIANGLE_DEMO3/qpuasm.js")

node_worker = f"""
const qpuasm = require('{qpuasm_abs_path}');
const fs = require('fs');

const tests = JSON.parse(fs.readFileSync('scratch/js_input.json', 'utf8'));
const results = {{}};

for (let t of tests) {{
    let words = [];
    const origLog = console.log;
    console.log = function(...args) {{
        const s = args.join(' ');
        const m = s.match(/(0x[0-9a-fA-F]{{8}}),\\s*(0x[0-9a-fA-F]{{8}})/);
        if (m) {{
            words.push([parseInt(m[1], 16), parseInt(m[2], 16)]);
        }}
    }};
    try {{
        qpuasm(t.lines.join('\\n') + '\\n', {{}});
    }} catch(e) {{
        console.log = origLog;
        console.error('Error in ' + t.var_name + ':', e.message);
        process.exit(1);
    }}
    console.log = origLog;
    results[t.enum_idx] = words;
}}

fs.writeFileSync('scratch/js_output.json', JSON.stringify(results));
"""
with open("scratch/run_qpuasm.js", "w") as f:
    f.write(node_worker)

print("4. Assembling all variants using reference qpuasm.js in Node.js...")
proc_js = subprocess.run(["node", "scratch/run_qpuasm.js"], capture_output=True, text=True)
if proc_js.returncode != 0:
    print("Node qpuasm failed:", proc_js.stderr, proc_js.stdout)
    exit(1)

with open("scratch/js_output.json", "r") as f:
    js_results = json.load(f)

print("5. Comparing AmigaOS vamos output vs Node.js qpuasm.js output...")
total_variants = len(variant_calls)
perfect_variants = 0
total_instructions = 0
matched_instructions = 0
diff_examples = []

for name, arr_name, var_id in variant_calls:
    enum_idx = enum_map[var_id]
    v_insts = vamos_variants.get(enum_idx, [])
    j_insts = js_results.get(str(enum_idx), [])
    
    if len(v_insts) != len(j_insts):
        diff_examples.append(f"Variant {enum_idx} ({name}): length mismatch vamos={len(v_insts)} vs js={len(j_insts)}")
        continue
    
    var_ok = True
    for k in range(len(v_insts)):
        total_instructions += 1
        vw0, vw1 = v_insts[k]
        jw0, jw1 = j_insts[k]
        if vw0 == jw0 and vw1 == jw1:
            matched_instructions += 1
        else:
            var_ok = False
            if len(diff_examples) < 15:
                diff_examples.append(f"Var {enum_idx} ({name}) line {k} [{arrays[arr_name][k]}]: vamos=(0x{vw0:08x}, 0x{vw1:08x}) js=(0x{jw0:08x}, 0x{jw1:08x})")

    if var_ok:
        perfect_variants += 1

print("\n" + "=" * 80)
print(f"VERIFICATION REPORT: AmigaOS C-Assembler vs Reference qpuasm.js")
print("=" * 80)
print(f"Total Shader Variants tested : {total_variants}")
print(f"Variants with 100% match     : {perfect_variants} / {total_variants}")
print(f"Total Instructions compared  : {total_instructions}")
print(f"Instructions exactly matched : {matched_instructions} / {total_instructions} ({matched_instructions/total_instructions*100:.2f}%)")
if diff_examples:
    print(f"\nDifferences observed ({len(diff_examples)}):")
    for d in diff_examples[:15]:
        print("  *", d)
print("=" * 80)
