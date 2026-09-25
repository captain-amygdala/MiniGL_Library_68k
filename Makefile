# miniglv3d-clean -- builds libminiglv3d.a, the MiniGLV3D driver archive that
# minigl-shared-library links into minigl.library (installed there as
# third_party/pistorm3d/libminiglv3d.a).
#
# This is MiniGLV3D/build_lib_gcc_nolog.sh as a Makefile: the same 27 sources,
# the same flags per group, the same archive member order, the same toolchain.
# The tree holds only the files that build reads -- 27 .c and 25 .h, the list
# gcc -MM gives for the script's own compile commands -- as committed in the
# MiniGLV3D git.
#
# Usage, from WSL (the toolchain lives there):
#     make -C <this directory>
#     make -C <this directory> OPT=-O3     (the script's optional $1)
#     make -C <this directory> clean
#
# LOCATION INDEPENDENT, AND THE ONE THING THAT COSTS: every path handed to the
# compiler is relative to this directory, so the output does not depend on
# where the repo lives. That matters because m68k-amigaos-gcc writes each
# object's -o argument, VERBATIM, into the object as its HUNK_UNIT name (the
# first bytes of the .o). build_lib_gcc_nolog.sh passes an ABSOLUTE -o,
# /mnt/d/v3d_driver/MiniGLV3D/obj_lib_gcc_nolog/<f>.o, so its archive carries
# that path 27 times, where this one carries obj_lib_gcc_nolog/<f>.o.
# Everything after the unit name is identical. To reproduce the script's
# archive byte for byte, give it the script's object directory:
#     make -C <this directory> -B OBJDIR=/mnt/d/v3d_driver/MiniGLV3D/obj_lib_gcc_nolog
# (-B because that directory already holds objects.) The unit name does not
# reach minigl.library -- the linker drops it -- so it changes this archive's
# bytes only, never the library's.

# Every path below is relative to this directory; refuse to run from anywhere
# else rather than silently build nothing or the wrong thing.
HERE := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
ifneq ($(HERE),$(CURDIR))
$(error run as: make -C $(HERE))
endif

# The script's toolchain. A different GCC or binutils build is a different
# compiler: byte identity holds for this one only.
# Default to /opt/amiga if present, otherwise fall back to $(HOME)/amiga-gcc-install
ifeq ($(wildcard /opt/amiga/bin/m68k-amigaos-gcc),)
PREFIX ?= $(HOME)/amiga-gcc-install
else
PREFIX ?= /opt/amiga
endif
CC     := $(PREFIX)/bin/m68k-amigaos-gcc
AR     := $(PREFIX)/bin/m68k-amigaos-ar
export PATH := $(PREFIX)/bin:$(PATH)

# Backend selection: v3d (Raspberry Pi 4 / VideoCore VI) or vc4 (Raspberry Pi 1/2/3/Zero / VideoCore IV)
BACKEND ?= vc4

DEBUG ?= 1

# Flags exactly as build_lib_gcc_nolog.sh sets them. -fno-strict-aliasing is
# REQUIRED (strict aliasing deletes v3d_commands.c's swivel-pattern writes);
# -fno-builtin-cos/-sin stops GCC fusing cos+sin into a cexp() nothing provides.
OPT       ?= -O2
ifeq ($(DEBUG),1)
OPTCFLAGS := -fno-strict-aliasing -fno-builtin-cos -fno-builtin-sin -finline-functions -DDEBUG -DMGLV3D_WINDOW_CACHE_PAIR=1
else
OPTCFLAGS := -fno-strict-aliasing -fno-builtin-cos -fno-builtin-sin -finline-functions -DMGLV3D_NO_LOGGING -DMGLV3D_WINDOW_CACHE_PAIR=1
endif
CPUFLAGS  := -mcpu=68020 -m68881 -mcrt=clib2

ifeq ($(BACKEND),v3d)
HW_DIR   := backend/v3d/hw
INCFLAGS := -Igl/include -Ibackend/v3d/include -Ibackend/v3d/hw -Ibackend/include -Ibackend/hw
HW_SRC   := v3d_assembler v3d_clbuf v3d_commands v3d_context v3d_device v3d_frame v3d_hw \
            v3d_mem_allocvec v3d_submit_timeout v3d_texture
LIB      ?= libminiglv3d.a
DBG_SRC  := v3d_debug
else ifeq ($(BACKEND),vc4)
HW_DIR   := backend/vc4/hw
INCFLAGS := -Igl/include -Ibackend/vc4/include -Ibackend/vc4/hw -Ibackend/include -Ibackend/hw
HW_SRC   := vc4_assembler vc4_clbuf vc4_commands vc4_context vc4_device vc4_frame vc4_hw \
            vc4_mem_allocvec vc4_mock_hw vc4_submit_timeout vc4_texture
LIB      ?= libminiglvc4.a
DBG_SRC  := vc4_debug
endif

OBJDIR ?= obj_lib_gcc_nolog_$(BACKEND)

GL_SRC   := aclip context draw fog glu hclip init matrix others texture \
            vertexarray vertexbuffer_min vertexelements viewport

GL_OBJS   := $(GL_SRC:%=$(OBJDIR)/%.o)
HW_OBJS   := $(HW_SRC:%=$(OBJDIR)/%.o)
DBG_OBJ   := $(OBJDIR)/$(DBG_SRC).o

# Member order is part of the archive's bytes: this is the script's order.
OBJS := $(GL_OBJS) $(HW_OBJS) $(DBG_OBJ)

HEADERS := $(wildcard gl/include/mgl/*.h gl/src/*.h backend/$(BACKEND)/include/*.h backend/$(BACKEND)/hw/*.h backend/include/*.h backend/hw/*.h)

DEMO_SRC := demos/cube_window_demo.c
DEMO_BIN := demos/cube_window_demo

.PHONY: all clean demo
all: $(LIB)
demo: $(DEMO_BIN)

$(DEMO_BIN): $(DEMO_SRC) $(LIB)
	$(CC) -std=c99 $(OPT) $(OPTCFLAGS) $(CPUFLAGS) -Igl/include -Ibackend/include $< -L. -lminigl$(BACKEND) -lm -o $@

$(GL_OBJS): $(OBJDIR)/%.o: gl/src/%.c $(HEADERS) Makefile | $(OBJDIR)
	$(CC) -std=c99 $(OPT) $(OPTCFLAGS) $(CPUFLAGS) $(INCFLAGS) -c $< -o $@

$(HW_OBJS): $(OBJDIR)/%.o: $(HW_DIR)/%.c $(HEADERS) Makefile | $(OBJDIR)
	$(CC) -std=c99 $(OPT) $(OPTCFLAGS) $(CPUFLAGS) $(INCFLAGS) -c $< -o $@

# Debug object gets -DDEBUG: its kprintf body must be real at link time.
$(DBG_OBJ): $(HW_DIR)/$(DBG_SRC).c $(HEADERS) Makefile | $(OBJDIR)
	$(CC) -std=c99 $(OPT) $(OPTCFLAGS) $(CPUFLAGS) -DDEBUG $(INCFLAGS) -c $< -o $@

# Built under a never-used name and renamed into place, as the script does:
# `ar rcs` onto a leftover archive MERGES into it instead of replacing it.
$(LIB): $(OBJS)
	rm -f $@.tmp$$$$ && $(AR) rcs $@.tmp$$$$ $(OBJS) && mv -f $@.tmp$$$$ $@

$(OBJDIR):
	mkdir -p $@

# Removes exactly what this Makefile builds, never the directory tree: OBJDIR
# can be pointed at another tree's object directory.
clean:
	rm -rf $(OBJDIR) obj_lib_gcc_nolog_v3d obj_lib_gcc_nolog_vc4 obj_lib_gcc_nolog $(LIB) libminiglv3d.a libminiglvc4.a $(DEMO_BIN)
