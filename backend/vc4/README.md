# VideoCore IV (VC4) Backend for PiStorm3D

Dieses Backend erweitert PiStorm3D / MiniGL um Unterstützung für VideoCore IV (VC4 / V3D 2.x) Hardware:
- **Raspberry Pi 3A+ / 3B / 3B+** (SoC BCM2837)
- **Raspberry Pi 2** (SoC BCM2836 / BCM2837)
- **Raspberry Pi 1 / Zero / Zero W** (SoC BCM2835)

## Unterschiede zum V3D 4.2 (Pi 4) Backend

1. **QPU Shader Architektur**:
   - V3D 4.2 nutzt die VideoCore VI ISA (`ldvary`, `wrtmuc`, `thrsw`, `vfpack`).
   - VC4 nutzt die klassische VideoCore IV VLIW-ISA (Dual-Issue ADD/MUL ALUs, Registerdateien A & B, Varyings über `r4`, Uniforms über `r5`, Texturen via `tmu0_s`/`tmu0_t`).
   - Referenz und Shader-Quellen: Siehe `depends/RaspberryPI/RPIZEROW/Sample_V3D_TEX_06/`.

2. **Textur-Tiling**:
   - V3D 4.2 nutzt UIF (Ultimate Image Format) mit XOR-Hashing.
   - VC4 nutzt das Broadcom **T-Format** (4x4 Micro-Tiles in 4K Sub-Tiles) und Lineartile.
   - Referenz: Siehe `depends/RaspberryPI/RPIZEROW/Sample_V3D_TEX_TFORMAT_08/`.

3. **Control List Pakete (CLE)**:
   - VC4 Binning & Rendering-Befehlsstrukturen unterscheiden sich in Bitbreiten und Formaten (siehe `depends/RaspberryPI/RPIZEROW/Sample_V3D_TEX_06/v3d.h`).

4. **Power-On & Mailbox**:
   - VC4 benötigt keine ASB-Grafikregister wie der Pi 4, sondern wird über Mailbox-Tag `0x00030002` (Domain 5) eingeschaltet.
