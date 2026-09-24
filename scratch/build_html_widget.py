import os

with open("scratch/mock_triangle_b64.txt") as f:
    b64_data = f.read().strip()

html_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>VC4 Mock Hardware 3D Triangle Viewer</title>
  <script src="https://www.gstatic.com/antigravity/web/dev/tailwindcss.min.js"></script>
  <style>
    body {{
      background: var(--background, #0f172a);
      color: var(--foreground, #f8fafc);
      font-family: ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    }}
    .pixelated {{
      image-rendering: pixelated;
    }}
  </style>
</head>
<body class="p-6">
  <div class="max-w-5xl mx-auto space-y-6">
    <!-- Header -->
    <div class="flex items-center justify-between border-b border-slate-700/60 pb-4">
      <div>
        <div class="flex items-center gap-3">
          <span class="px-2.5 py-1 text-xs font-semibold rounded bg-emerald-500/20 text-emerald-400 border border-emerald-500/30">
            MOCK HARDWARE ACTIVE
          </span>
          <span class="text-xs text-slate-400 font-mono">V3D 2.1 (BCM2837 / VC4)</span>
        </div>
        <h1 class="text-2xl font-bold mt-1 text-white tracking-tight">VC4 Hardware 3D Rendering Pipeline</h1>
        <p class="text-sm text-slate-400">Gouraud-interpoliertes Dreieck gerendert unter AmigaOS 68040 (vamos) via VC4 Mock GPU</p>
      </div>
      <div class="text-right font-mono text-xs text-slate-400 space-y-1">
        <div>Resolution: <span class="text-slate-200">256 × 256 RGBA32</span></div>
        <div>Pixels Rendered: <span class="text-emerald-400 font-bold">20,000 px</span></div>
        <div>Status: <span class="text-emerald-400">BFC=1, RFC=1 (OK)</span></div>
      </div>
    </div>

    <!-- Main Content Grid -->
    <div class="grid grid-cols-1 md:grid-cols-12 gap-6">
      <!-- Left Column: Interactive Framebuffer Canvas -->
      <div class="md:col-span-6 bg-slate-900/80 border border-slate-800 rounded-xl p-5 shadow-xl flex flex-col items-center">
        <div class="w-full flex items-center justify-between mb-4">
          <h2 class="text-sm font-semibold uppercase tracking-wider text-slate-300">Target Framebuffer</h2>
          <div class="flex items-center gap-2">
            <button id="zoomBtn" class="px-2.5 py-1 text-xs bg-slate-800 hover:bg-slate-700 text-slate-300 rounded border border-slate-700 transition">
              Zoom: <span id="zoomLevel">1x</span>
            </button>
            <button id="gridBtn" class="px-2.5 py-1 text-xs bg-slate-800 hover:bg-slate-700 text-slate-300 rounded border border-slate-700 transition">
              Tile Grid
            </button>
          </div>
        </div>

        <!-- Canvas Container -->
        <div class="relative bg-slate-950 rounded-lg p-2 border border-slate-800 shadow-inner flex items-center justify-center overflow-hidden w-[272px] h-[272px]">
          <img id="fbImage" src="data:image/png;base64,{b64_data}" 
               alt="Rendered VC4 Triangle"
               class="pixelated transition-transform origin-center cursor-crosshair rounded"
               width="256" height="256" style="transform: scale(1);" />
          
          <!-- 4x4 Tile Grid Overlay -->
          <div id="tileOverlay" class="absolute inset-2 grid grid-cols-4 grid-rows-4 pointer-events-none hidden border border-dashed border-cyan-400/40">
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T0</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T1</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T2</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T3</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T4</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T5</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T6</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T7</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T8</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T9</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T10</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T11</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T12</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T13</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T14</div>
            <div class="border border-dashed border-cyan-400/30 flex items-start justify-start p-0.5 text-[8px] text-cyan-300">T15</div>
          </div>
        </div>

        <!-- Pixel Inspector Live Data -->
        <div class="w-full mt-4 bg-slate-950/80 rounded-lg p-3 border border-slate-800 text-xs font-mono">
          <div class="flex items-center justify-between text-slate-400 mb-1">
            <span>Pixel Inspector:</span>
            <span id="pixelCoords">X: 128, Y: 128</span>
          </div>
          <div class="flex items-center gap-3">
            <div id="colorSwatch" class="w-6 h-6 rounded border border-slate-700 shadow-sm" style="background-color: #6a4030;"></div>
            <div id="colorHex" class="text-slate-200 font-bold">RGBA: (106, 64, 48, 255)</div>
          </div>
        </div>
      </div>

      <!-- Right Column: Hardware Pipeline Inspector -->
      <div class="md:col-span-6 bg-slate-900/80 border border-slate-800 rounded-xl p-5 shadow-xl flex flex-col">
        <!-- Tabs -->
        <div class="flex border-b border-slate-800 gap-2 mb-4">
          <button id="tabPacketsBtn" class="px-3 py-1.5 text-xs font-medium text-cyan-400 border-b-2 border-cyan-400 pb-2">
            V3D Control Lists
          </button>
          <button id="tabShaderBtn" class="px-3 py-1.5 text-xs font-medium text-slate-400 hover:text-slate-200 border-b-2 border-transparent pb-2 transition">
            QPU Shader Binary
          </button>
          <button id="tabVerticesBtn" class="px-3 py-1.5 text-xs font-medium text-slate-400 hover:text-slate-200 border-b-2 border-transparent pb-2 transition">
            Vertices & State
          </button>
        </div>

        <!-- Tab 1: Control Lists -->
        <div id="tabPackets" class="space-y-3 text-xs font-mono flex-1 overflow-y-auto max-h-[320px]">
          <div class="p-2.5 rounded bg-slate-950 border border-slate-800/80 space-y-1">
            <div class="text-amber-400 font-semibold flex items-center justify-between">
              <span>Binning Control List (57 Bytes)</span>
              <span class="text-[10px] text-slate-500">CT0CA ➔ CT0EA</span>
            </div>
            <div class="text-slate-300 space-y-0.5 text-[11px]">
              <div class="text-slate-400">112: TILE_BINNING_MODE_CFG (16B) [4x4 tiles, auto-init]</div>
              <div class="text-slate-400">102: CLIPWINDOW (9B) [0, 0 to 256, 256]</div>
              <div class="text-slate-400">105: CLIPPER_XY_SCALING (9B) [256.0, 256.0]</div>
              <div class="text-slate-400"> 96: CFG_BITS (4B) [Forward/Reverse facing]</div>
              <div class="text-slate-400">  6: START_TILE_BINNING (1B)</div>
              <div class="text-emerald-400 font-semibold"> 65: NV_SHADER_STATE (5B) ➔ Record at 0x000B2478</div>
              <div class="text-slate-400"> 56: PRIM_LIST_FORMAT (2B) [Triangles, 16-bit idx]</div>
              <div class="text-cyan-400 font-semibold"> 33: VERTEX_ARRAY_PRIMS (10B) [3 vertices, mode=TRI]</div>
              <div class="text-slate-400">  4: FLUSH (1B)</div>
            </div>
          </div>

          <div class="p-2.5 rounded bg-slate-950 border border-slate-800/80 space-y-1">
            <div class="text-amber-400 font-semibold flex items-center justify-between">
              <span>Render Control List (89 Bytes)</span>
              <span class="text-[10px] text-slate-500">CT1CA ➔ CT1EA</span>
            </div>
            <div class="text-slate-300 space-y-0.5 text-[11px]">
              <div class="text-slate-400">113: TILE_RENDERING_MODE_CFG (11B) [FB=0x011B06C0, RGBA8]</div>
              <div class="text-cyan-400">114: TILE_RENDERING_CLEAR_COLORS (14B) [Dark Slate Blue]</div>
              <div class="text-slate-400">115: TILE_COORDINATES (3B) [Tile (0,0) ... Tile (3,3)]</div>
              <div class="text-slate-400"> 17: BRANCH_TO_SUB_LIST (5B) [Points to tile binned bins]</div>
              <div class="text-emerald-400"> 25: STORE_MS_RESOLVED_COLOR_AND_EOF (1B)</div>
            </div>
          </div>
        </div>

        <!-- Tab 2: Shader -->
        <div id="tabShader" class="hidden space-y-2 text-xs font-mono flex-1 overflow-y-auto max-h-[320px]">
          <div class="text-slate-400 text-[11px] mb-1">Variant: <span class="text-emerald-400">V3D_SHADER_VARIANT_FRAGMENT_UNTEXTURED_SMOOTH</span> (Gouraud Interpolation)</div>
          <div class="p-2.5 bg-slate-950 rounded border border-slate-800 text-[11px] space-y-1 text-slate-300">
            <div><span class="text-slate-500">0x00:</span> mov r0, vary ; mov r3.8d, 1.0</div>
            <div><span class="text-slate-500">0x08:</span> fadd r0, r0, r5 ; mov r1, vary ; sbwait</div>
            <div><span class="text-slate-500">0x10:</span> fadd r1, r1, r5 ; mov r2, vary</div>
            <div><span class="text-slate-500">0x18:</span> fadd r2, r2, r5 ; mov r3.8a, r0</div>
            <div><span class="text-slate-500">0x20:</span> nop ; mov r3.8b, r1</div>
            <div><span class="text-slate-500">0x28:</span> nop ; mov r3.8c, r2</div>
            <div><span class="text-slate-500">0x30:</span> mov tlbc, r3 ; nop ; thrend</div>
            <div><span class="text-slate-500">0x38:</span> nop</div>
            <div><span class="text-slate-500">0x40:</span> nop ; nop ; sbdone</div>
          </div>
          <div class="text-[10px] text-slate-500">
            Dual-issue QPU instructions execute 16-way SIMD barycentric interpolation directly into the tile color buffer.
          </div>
        </div>

        <!-- Tab 3: Vertices -->
        <div id="tabVertices" class="hidden space-y-2 text-xs font-mono flex-1 overflow-y-auto max-h-[320px]">
          <div class="p-2.5 bg-slate-950 rounded border border-slate-800 text-[11px] space-y-1.5">
            <div class="text-red-400 font-semibold">V0 (Top):</div>
            <div class="text-slate-300 pl-3">Pos: (128, 28, 0.5) ➔ 12.4 fixed: (2048, 448)<br>Color: RGBA(1.0, 0.0, 0.0, 1.0) [Pure Red]</div>

            <div class="text-emerald-400 font-semibold pt-1">V1 (Bottom-Left):</div>
            <div class="text-slate-300 pl-3">Pos: (28, 228, 0.5) ➔ 12.4 fixed: (448, 3648)<br>Color: RGBA(0.0, 1.0, 0.0, 1.0) [Pure Green]</div>

            <div class="text-blue-400 font-semibold pt-1">V2 (Bottom-Right):</div>
            <div class="text-slate-300 pl-3">Pos: (228, 228, 0.5) ➔ 12.4 fixed: (3648, 3648)<br>Color: RGBA(0.0, 0.0, 1.0, 1.0) [Pure Blue]</div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <script>
    const fbImage = document.getElementById('fbImage');
    const zoomBtn = document.getElementById('zoomBtn');
    const zoomLevel = document.getElementById('zoomLevel');
    const gridBtn = document.getElementById('gridBtn');
    const tileOverlay = document.getElementById('tileOverlay');
    const pixelCoords = document.getElementById('pixelCoords');
    const colorSwatch = document.getElementById('colorSwatch');
    const colorHex = document.getElementById('colorHex');

    let currentZoom = 1;
    zoomBtn.addEventListener('click', () => {{
      currentZoom = currentZoom === 1 ? 1.5 : (currentZoom === 1.5 ? 2 : 1);
      zoomLevel.textContent = currentZoom + 'x';
      fbImage.style.transform = `scale(${{currentZoom}})`;
    }});

    gridBtn.addEventListener('click', () => {{
      tileOverlay.classList.toggle('hidden');
      gridBtn.classList.toggle('bg-cyan-900/50');
      gridBtn.classList.toggle('border-cyan-500');
    }});

    // Tabs
    const tabPacketsBtn = document.getElementById('tabPacketsBtn');
    const tabShaderBtn = document.getElementById('tabShaderBtn');
    const tabVerticesBtn = document.getElementById('tabVerticesBtn');
    const tabPackets = document.getElementById('tabPackets');
    const tabShader = document.getElementById('tabShader');
    const tabVertices = document.getElementById('tabVertices');

    function selectTab(btn, tab) {{
      [tabPacketsBtn, tabShaderBtn, tabVerticesBtn].forEach(b => {{
        b.className = 'px-3 py-1.5 text-xs font-medium text-slate-400 hover:text-slate-200 border-b-2 border-transparent pb-2 transition';
      }});
      [tabPackets, tabShader, tabVertices].forEach(t => t.classList.add('hidden'));

      btn.className = 'px-3 py-1.5 text-xs font-medium text-cyan-400 border-b-2 border-cyan-400 pb-2';
      tab.classList.remove('hidden');
    }}

    tabPacketsBtn.addEventListener('click', () => selectTab(tabPacketsBtn, tabPackets));
    tabShaderBtn.addEventListener('click', () => selectTab(tabShaderBtn, tabShader));
    tabVerticesBtn.addEventListener('click', () => selectTab(tabVerticesBtn, tabVertices));
  </script>
</body>
</html>
"""

target_path = "/home/claude/.gemini/antigravity/brain/e58660c7-c26c-4817-a810-9aa3de15d5ff/vc4_triangle_viewer.html"
with open(target_path, "w") as f:
    f.write(html_content)

print(f"Written {len(html_content)} bytes to {target_path}")
