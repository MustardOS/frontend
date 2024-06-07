#!/bin/bash

# Device options are listed in common/help.h.
# Targets include:
#       RG28XX
#       RG35XXH
#       RG35XXOG
#       RG35XXPLUS
#       RG35XXSP
#       RG35XX2024
export DEVICE=RG35XXPLUS
 
# This is the location of your installed Batocera Lite SDK Toolchain.
# If this is incorrect, point it to your directory.
export XTOOL=$HOME/x-tools
export XBIN=$XTOOL/bin
 
export PATH="${PATH}:$XBIN"
 
export SYSROOT=$XTOOL/$XHOST/$XHOST/sysroot
export DESTDIR=$SYSROOT
 
export CC=$XBIN/$XHOST-gcc
export CXX=$XBIN/$XHOST-g++
export AR=$XBIN/$XHOST-ar
export LD=$XBIN/$XHOST-ld
export STRIP=$XBIN/$XHOST-strip
 
export LD_LIBRARY_PATH="$SYSROOT/usr/lib"
 
export CPP_FLAGS="--sysroot=$SYSROOT -I$SYSROOT/usr/include"
export LD_FLAGS="-L$SYSROOT -L$SYSROOT/lib -L$SYSROOT/usr/lib -L$SYSROOT/usr/local/lib -L$SYSROOT/usr/include/sound"
 
export CPPFLAGS=$CPP_FLAGS
export LDFLAGS=$LD_FLAGS
 
export CFLAGS="-marm -mfpu=neon -mfloat-abi=hard $CPP_FLAGS"
export CCFLAGS=$CPP_FLAGS
export CXXFLAGS=$CPP_FLAGS
 
export INC_DIR=$CPP_FLAGS
export LIB_DIR=$LD_FLAGS
 
export ARMABI=$XHOST
export TOOLCHAIN_DIR=$XTOOL/$XHOST
 
export PKG_CONFIG_PATH=$SYSROOT/usr/lib/pkgconfig
export PKG_CONF_PATH=$XBIN/pkgconf
 
export CROSS_COMPILE=$XBIN/$XHOST-
 
export SDL_CONFIG=$SYSROOT/usr/bin/sdl-config
export FREETYPE_CONFIG=$SYSROOT/usr/bin/freetype-config
