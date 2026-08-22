# MicroPython embed-port generation for Graphite (Phase 6B.1).
#
# MicroPython has no build system this project can call directly. This
# makefile runs upstream's `embed.mk`, which GENERATES a self-contained
# tree of .c/.h (py/, extmod/, shared/runtime/, genhdr/, port/) that our
# CMake then compiles as the `micropython` static library.
#
# It is invoked from CMakeLists.txt at CONFIGURE time, because CMake
# needs the source list before it can define a target:
#
#   make -C drivers/micropython_port -f micropython_embed.mk \
#        BUILD=<bindir>/micropython_embed_build \
#        PACKAGE_DIR=<bindir>/micropython_embed
#
# It must run with this directory as the working directory: upstream's
# embed.mk puts `-I.` on CFLAGS and that is how it finds our
# mpconfigport.h. Everything else comes from the submodule.

# The submodule, pinned by the superproject. Relative so it works from
# any checkout path.
MICROPYTHON_TOP = ../micropython

# Modules from extmod/ that we want on top of the py/ core.
#
# Upstream's embed.mk copies py/*.c and exactly one extmod header, so an
# extmod module has to be added on two axes: to SRC_QSTR (which drives
# BOTH the qstr scan and the MP_REGISTER_MODULE collection — both derive
# from the same preprocessed blob), and to the copy step that populates
# the package.
#
# SRC_QSTR is set BEFORE the include on purpose. py.mk appends to it with
# `+=`, and make expands an explicit rule's prerequisites when it parses
# the rule — so appending after the include would be too late for the
# qstr.i.last rule that consumes it.
#
# json is here because §4.6's periodic-table walkthrough needs it to
# parse a bundled dataset; §5 makes it part of 6B.1's acceptance.
EXTMOD_SRC = extmod/modjson.c
SRC_QSTR = $(addprefix $(MICROPYTHON_TOP)/,$(EXTMOD_SRC))

# Our own `calc` module (Phase 6B.3). Unlike the extmod entry above it is
# NOT copied into the package: CMake compiles it from src/ as part of the
# firmware, alongside every other .c we own. Only the generator needs to
# see it, and for exactly two things — MP_QSTR_* (so the names it uses
# exist in the qstr pool) and MP_REGISTER_MODULE (so `calc` appears in
# genhdr/moduledefs.h, which py/objmodule.c includes). The resulting
# reference to `calc_user_cmodule` from the micropython lib resolves
# against our object at link.
#
# makeqstrdefs.py sanitizes ".." to "@@" and "/" to "__" when it names its
# per-source fragments, so a source outside MICROPYTHON_TOP is fine.
#
# -I is needed because the generator preprocesses this file with the HOST
# compiler: everything it includes must be reachable and host-clean, which
# is why calc_api.h is plain C over <stddef.h> and nothing else.
GRAPHITE_SRC = ../../src
CFLAGS += -I$(GRAPHITE_SRC)
SRC_QSTR += $(GRAPHITE_SRC)/scripting/mp_calc_module.c

include $(MICROPYTHON_TOP)/ports/embed/embed.mk

# Append to the package after upstream's copy step has created the dirs.
# `all` already exists as the default goal; adding a prerequisite to it
# is how make lets us extend a target we do not own.
all: graphite-embed-extras

.PHONY: graphite-embed-extras
graphite-embed-extras: micropython-embed-package
	$(ECHO) "- extmod (graphite)"
	$(Q)$(CP) $(addprefix $(TOP)/,$(EXTMOD_SRC)) $(PACKAGE_DIR)/extmod
