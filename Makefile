PS3DEV ?= /mnt/d/ps3dev-sdk
PSL1GHT := $(PS3DEV)
PORTLIBS := $(PS3DEV)/portlibs/ppu

export PS3DEV
export PSL1GHT
export PORTLIBS

CXX := $(PS3DEV)/ppu/bin/ppu-g++
STRIP := $(PS3DEV)/ppu/bin/ppu-strip
SPRX := $(PS3DEV)/bin/sprxlinker
SELF := $(PS3DEV)/bin/make_self

TARGET := cannonball_ps3

CORE_SRCS := \
	src/main/main.cpp \
	src/main/romloader.cpp \
	src/main/roms.cpp \
	src/main/trackloader.cpp \
	src/main/utils.cpp \
	src/main/video.cpp \
	src/main/directx/ffeedback.cpp \
	src/main/sdl2/audio.cpp \
	src/main/sdl2/input.cpp \
	src/main/sdl2/renderbase.cpp \
	src/main/sdl2/rendersurface.cpp \
	src/main/sdl2/timer.cpp \
	$(wildcard src/main/engine/*.cpp) \
	$(wildcard src/main/engine/audio/*.cpp) \
	$(wildcard src/main/frontend/*.cpp) \
	$(wildcard src/main/hwaudio/*.cpp) \
	$(wildcard src/main/hwvideo/*.cpp)

PS3_SRCS := $(wildcard ps3/*.cpp)
SRCS := $(CORE_SRCS) $(PS3_SRCS)
PREBUILT_OBJS := ps3/prebuilt/SDL_PSL1GHTrender.o
OBJS := $(patsubst %.cpp,build/%.o,$(SRCS)) $(PREBUILT_OBJS)

INCLUDES := -Isrc/main -Ips3 \
	-I$(PS3DEV)/ppu/include -I$(PS3DEV)/ppu/ppu/include \
	-I$(PORTLIBS)/include -I$(PORTLIBS)/include/SDL2 -I$(PORTLIBS)/include/freetype2
DEFS := -D__PS3__ -D__PSL1GHT__ -DPS3_PORT -DSDL_MAIN_HANDLED -DCOMPILE_SOUND_CODE -D__BIG_ENDIAN__
CXXFLAGS := -O2 -mcpu=cell -m64 -std=gnu++17 -fno-strict-aliasing -fsigned-char \
	-fno-tree-vectorize -fno-tree-slp-vectorize -fcommon \
	-Wno-write-strings -Wno-register $(DEFS) $(INCLUDES)
LDFLAGS := -m64 -Wl,--wrap=rsxSetTransferScaleSurface -L$(PS3DEV)/ppu/lib -L$(PS3DEV)/ppu/ppu/lib -L$(PORTLIBS)/lib
LIBS := -lSDL2_ttf -lfreetype -lpng -lSDL2 -lgcm_sys -lio -laudio -lsysutil -lsysmodule -lrsx -lrt -llv2 -lz -lm -lc

PS3_CXXFLAGS := $(CXXFLAGS) -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-optimize-sibling-calls

.PHONY: all clean self

all: $(TARGET).elf

build/ps3/%.o: ps3/%.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX-PS3] $<"
	@$(CXX) $(PS3_CXXFLAGS) -c $< -o $@

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	@echo "[LD] $@"
	@$(CXX) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

self: $(TARGET).elf
	@$(STRIP) $(TARGET).elf -o $(TARGET).stripped.elf
	@$(SPRX) $(TARGET).stripped.elf
	@$(SELF) $(TARGET).stripped.elf $(TARGET).self

clean:
	rm -rf build $(TARGET).elf $(TARGET).stripped.elf $(TARGET).self
