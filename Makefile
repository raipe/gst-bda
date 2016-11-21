# Compiles BDA plugin using MinGW

PKG_32 = i686-w64-mingw32-pkg-config
PKG_64 = x86_64-w64-mingw32-pkg-config

CC_32 = i686-w64-mingw32-g++
CC_64 = x86_64-w64-mingw32-g++

STRIP_32 = i686-w64-mingw32-strip
STRIP_64 = x86_64-w64-mingw32-strip

LDFLAGS = -lole32 -lstrmiids -lquartz -loleaut32 -luuid -lksguid
LDFLAGS_32 = $(LDFLAGS) $(shell $(PKG_32) --libs gstreamer-1.0 gstreamer-base-1.0)
LDFLAGS_64 = $(LDFLAGS) $(shell $(PKG_64) --libs gstreamer-1.0 gstreamer-base-1.0)

CXXFLAGS = -Wall -Wextra -DMINGW_HAS_SECURE_API=1 \
    -DVERSION="\"0.0.1\"" -DGST_LICENSE="\"LGPL\"" \
    -DGST_PACKAGE_NAME="\"GStreamer BDA Plugin\"" \
    -DGST_PACKAGE_ORIGIN="\"https://github.com/raipe/gst-bda\"" \
    -DPACKAGE="\"gstreamer\""
CXXFLAGS_32 = $(CXXFLAGS) $(shell $(PKG_32) --cflags gstreamer-1.0)
CXXFLAGS_64 = $(CXXFLAGS) $(shell $(PKG_64) --cflags gstreamer-1.0)

SRC := $(wildcard *.cpp)
OBJ_32 := $(patsubst %.cpp, build/32/%.o, $(SRC))
OBJ_64 := $(patsubst %.cpp, build/64/%.o, $(SRC))

BDA_32 := build/32/libgstbdasrc.dll
BDA_64 := build/64/libgstbdasrc.dll

all: init $(BDA_32) $(BDA_64)

init:
	@mkdir -p build/32
	@mkdir -p build/64

clean:
	rm -rf build

build/32/%.o: %.cpp
	$(CC_32) $(CXXFLAGS_32) -c $< -o $@

build/64/%.o: %.cpp
	$(CC_64) $(CXXFLAGS_64) -c $< -o $@

$(BDA_32): $(OBJ_32)
	$(CC_32) -shared $(CXXFLAGS_32) $(OBJ_32) $(LDFLAGS_32) -o $@
	$(STRIP_32) $@

$(BDA_64): $(OBJ_64)
	$(CC_64) -shared $(CXXFLAGS_64) $(OBJ_64) $(LDFLAGS_64) -o $@
	$(STRIP_64) $@
