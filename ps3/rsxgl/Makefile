.SUFFIXES:

PS3DEV ?= /usr/local/ps3dev
PSL1GHT ?= /home/aleksandr/PSL1GHT
export PS3DEV PSL1GHT
include $(PSL1GHT)/ppu_rules

TARGET := RallySimulatorRSXGL
BUILD := build
SOURCES := source
DATA := data
SHADERS := shaders_gl
INCLUDES := include
PKGFILES := pkgfiles
TITLE := Rally Simulator RSXGL
APPID := RALLYGL01
CONTENTID := UP0001-$(APPID)_00-0000000000000000
CGC := $(PS3DEV)/bin/cgc
NV40ASM := $(PS3DEV)/bin/nv40asm
NV40C := $(PS3DEV)/bin/nv40c
XXD := xxd

CFLAGS := -O2 -Wall -mcpu=cell $(MACHDEP) $(INCLUDE)
CXXFLAGS := $(CFLAGS) -std=c++14
LDFLAGS := $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS := -lGL -lEGL -lrsx -lgcm_sys -lnet -lio -lsysutil -lsysmodule -lrt -llv2 -lm -lsimdmath
LIBDIRS := $(PS3DEV)/portlibs/ppu

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) $(foreach dir,$(DATA),$(CURDIR)/$(dir)) $(foreach dir,$(SHADERS),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
export BUILDDIR := $(CURDIR)/$(BUILD)
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
VCGFILES := $(foreach dir,$(SHADERS),$(notdir $(wildcard $(dir)/*.vert)))
FCGFILES := $(foreach dir,$(SHADERS),$(notdir $(wildcard $(dir)/*.frag)))
VPOFILES := $(VCGFILES:.vert=.vpo)
FPOFILES := $(FCGFILES:.frag=.fpo)
export VPOHFILES := $(VCGFILES:.vert=_vert.h)
export FPOHFILES := $(FCGFILES:.frag=_frag.h)
export OFILES := $(addsuffix .o,$(VPOFILES)) $(addsuffix .o,$(FPOFILES)) $(CPPFILES:.cpp=.o)
export LD := $(CXX)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) $(LIBPSL1GHT_INC) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) $(LIBPSL1GHT_LIB)
.PHONY: all clean pkg
all: $(BUILD)
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@$(MAKE) --no-print-directory $(OUTPUT).self
pkg: $(BUILD) $(OUTPUT).pkg
clean:
	@rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).fake.self $(OUTPUT).pkg $(basename $(OUTPUT)).gnpdrm.pkg
else
DEPENDS := $(OFILES:.o=.d)
all: $(VPOHFILES) $(FPOHFILES) $(OFILES) $(OUTPUT).elf $(OUTPUT).self
%.vpo: %.vert
	@CGC=$(CGC) NV40ASM=$(NV40ASM) $(NV40C) -v -x glsl -o $@ ../$(SHADERS)/$(notdir $<)
%.fpo: %.frag
	@CGC=$(CGC) NV40ASM=$(NV40ASM) $(NV40C) -f -x glsl -o $@ ../$(SHADERS)/$(notdir $<)
%_vert.h: %.vert
	@cp -f $< $(CURDIR)/
	@cd $(CURDIR) && $(XXD) -i $(notdir $<) > $@
%_frag.h: %.frag
	@cp -f $< $(CURDIR)/
	@cd $(CURDIR) && $(XXD) -i $(notdir $<) > $@
-include $(DEPENDS)
endif
