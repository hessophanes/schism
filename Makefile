# =============================================================================
# Default target prelude

.PHONY: all clean

all:

# =============================================================================
# Common (hypervisor-independent) targets

EASY_TARGETS := precompilation expand reparent measure generate_config layout_memory layout_map pagetables combine
EASY_SRC := $(patsubst %,src/%.c,$(EASY_TARGETS))

TARGETS := $(EASY_TARGETS)
CLEAN_CBI_PATTERNS := precompilation_* generate_config_*

# =============================================================================
# XML library

XML_DEFINITION := include/xml_definition.txt
XML_STRUCT_HDR := include/_gen_xml_structs.h

XML_LIBRARY := $(wildcard xmllib/*)

LIB := lib/libxml41.a
LIBSRC := lib/scenario.c lib/layouter.c lib/elf.c $(wildcard lib/xml/*.c)

HDR := include/scenario.h include/xml.h $(XML_STRUCT_HDR)

# =============================================================================
# Default flags

CFLAGS ?= -Wall -Wextra -g -O0 -Iinclude -Llib

# =============================================================================
# =============================================================================
# ^^^  Definitions  ^^^					vvv  Build Recipes  vvv
# =============================================================================
# =============================================================================

# =============================================================================
# Common (hypervisor-independent) targets

$(XML_STRUCT_HDR): $(XML_DEFINITION) tools/xml_prepare.py
	$(filter %.py,$^)

$(LIB): $(patsubst %.c,%.o,$(LIBSRC)) $(HDR)
	ar rs $@ $(filter %.o,$^)

$(patsubst %.c,%.o,$(EASY_SRC) $(LIBSRC)): %.o: %.c $(HDR)
	gcc $< $(CFLAGS) -c -o $@

$(EASY_TARGETS): %: src/%.c $(LIB) $(HDR)
	gcc $< $(CFLAGS) -lxml41 -o $@

# =============================================================================
# If we have "O=...", include output targets

ifneq ($(O),)
override O := $(shell realpath $(patsubst %/,%,$(O)))

include Makefile.output
endif

# =============================================================================
# Phony targets

all: $(TARGETS)

clean:
	rm -f lib/*.o lib/*.a *.o $(TARGETS) $(CLEAN_CBI_PATTERNS)
