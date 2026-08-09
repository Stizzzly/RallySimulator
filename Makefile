.SUFFIXES:

# Defaults for the Debian WSL installation detected on this machine. They can
# still be overridden by exporting PS3DEV/PSL1GHT before invoking make.
PS3DEV  ?= /usr/local/ps3dev
PSL1GHT ?= /home/aleksandr/PSL1GHT
export PS3DEV PSL1GHT

ifeq ($(strip $(PSL1GHT)),)
$(error Please set PSL1GHT, for example: export PSL1GHT=/home/aleksandr/PSL1GHT)
endif

include $(PSL1GHT)/ppu_rules

# The checked-out PSL1GHT tree contains headers/rules; compiled system
# libraries are installed by ps3toolchain under the PS3DEV prefix.
export LIBPSL1GHT_LIB := -L$(PS3DEV)/ppu/lib

TARGET   := RallySimulatorPS3
BUILD    := build
SOURCES  := .
INCLUDES :=

TITLE    := Rally Simulator PS3
APPID    := RALLY0001
CONTENTID := UP0001-$(APPID)_00-0000000000000000
PKGFILES := pkgfiles

CFLAGS   := -O2 -Wall -mcpu=cell $(MACHDEP) $(INCLUDE)
CXXFLAGS := $(CFLAGS) -std=c++14
LDFLAGS  := $(MACHDEP) -Wl,-Map,$(notdir $@).map

# SDL2 supplies the timer/event layer. tiny3d owns all RSX drawing.
LIBS     := -ltiny3d -lSDL2 -lrsx -lgcm_sys -lio -lsysutil -laudio -lrt -llv2 -lm
LIBDIRS  := $(PS3DEV)/portlibs/ppu

export INCLUDE   := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                    $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                    $(LIBPSL1GHT_INC) -I$(CURDIR)/$(BUILD)
export LIBPATHS  := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) $(LIBPSL1GHT_LIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export VPATH    := $(CURDIR)/$(SOURCES)
export DEPSDIR  := $(CURDIR)/$(BUILD)
export BUILDDIR := $(CURDIR)/$(BUILD)

CPPFILES := $(notdir $(wildcard $(SOURCES)/*.cpp))
CFILES   := $(notdir $(wildcard $(SOURCES)/*.c))
export LD := $(CXX)
export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o)

.PHONY: all clean run pkg

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@$(MAKE) --no-print-directory $(OUTPUT).self

$(OUTPUT).self: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

clean:
	@rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).fake.self $(OUTPUT).pkg $(basename $(OUTPUT)).gnpdrm.pkg

run: $(OUTPUT).self
	@ps3load $(OUTPUT).self

pkg: $(BUILD) $(OUTPUT).pkg

else

DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)

endif
