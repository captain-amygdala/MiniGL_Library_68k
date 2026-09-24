
const qpuasm = require('/home/claude/antigravity/v3d_minigl/depends/RaspberryPI/RPI2/RPI2_TRIANGLE_DEMO3/qpuasm.js');
const fs = require('fs');

const tests = JSON.parse(fs.readFileSync('scratch/js_input.json', 'utf8'));
const results = {};

for (let t of tests) {
    let words = [];
    const origLog = console.log;
    console.log = function(...args) {
        const s = args.join(' ');
        const m = s.match(/(0x[0-9a-fA-F]{8}),\s*(0x[0-9a-fA-F]{8})/);
        if (m) {
            words.push([parseInt(m[1], 16), parseInt(m[2], 16)]);
        }
    };
    try {
        qpuasm(t.lines.join('\n') + '\n', {});
    } catch(e) {
        console.log = origLog;
        console.error('Error in ' + t.var_name + ':', e.message);
        process.exit(1);
    }
    console.log = origLog;
    results[t.enum_idx] = words;
}

fs.writeFileSync('scratch/js_output.json', JSON.stringify(results));
