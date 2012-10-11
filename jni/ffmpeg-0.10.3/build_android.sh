#!/bin/bash

######################################################
# Usage:
#   put this script in top of FFmpeg source tree
#   ./build_android
#
# It generates binary for following architectures:
#     ARMv6 
#     ARMv6+VFP 
#     ARMv7+VFPv3-d16 (Tegra2) 
#     ARMv7+Neon (Cortex-A8)
#
# Customizing:
# 1. Feel free to change ./configure parameters for more features
# 2. To adapt other ARM variants
#       set $CPU and $OPTIMIZE_CFLAGS 
#       call build_one
######################################################

NDK=~/android-ndk-r7c
PLATFORM=$NDK/platforms/android-8/arch-arm/
PREBUILT=$NDK/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86


function dirty_build
{
make  -j4 

$PREBUILT/bin/arm-linux-androideabi-ar d libavcodec/libavcodec.a inverse.o

make install

#Link all library to 1 libffmpeg.so
$PREBUILT/bin/arm-linux-androideabi-ld -rpath-link=$PLATFORM/usr/lib \
    -L$PLATFORM/usr/lib  -L$PREFIX/lib -L$X264_PREFIX/lib\
    -soname libffmpeg.so \
    -shared -nostdlib  -z,noexecstack  \
    --whole-archive --no-undefined -o $PREFIX/libffmpeg.so \
    --warn-once  --dynamic-linker=/system/bin/linker \
    $PREBUILT/lib/gcc/arm-linux-androideabi/4.4.3/libgcc.a \
    -lc -lm -lz -ldl -llog \
    -Bstatic \
    -lavcodec -lavformat \
    -lavutil -lswscale \
    -lswresample -lavfilter \
    -lavdevice -lx264 

}

function build_one
{
./configure --target-os=linux \
    --prefix=$PREFIX \
    --enable-cross-compile \
    --enable-version3 \
    --enable-gpl \
    --enable-memalign-hack \
    --enable-asm \
    --extra-libs="-lgcc" \
    --arch=arm \
    --cc=$PREBUILT/bin/arm-linux-androideabi-gcc \
    --cross-prefix=$PREBUILT/bin/arm-linux-androideabi- \
    --nm=$PREBUILT/bin/arm-linux-androideabi-nm \
    --sysroot=$PLATFORM \
    --extra-cflags=" -O3 -fpic -DANDROID -DHAVE_SYS_UIO_H=1 -Dipv6mr_interface=ipv6mr_ifindex -fasm -Wno-psabi -fno-short-enums  -fno-strict-aliasing -finline-limit=300 -I$X264_PREFIX/include $OPTIMIZE_CFLAGS " \
    --disable-shared \
    --enable-static \
    --enable-runtime-cpudetect \
    --extra-ldflags="-Wl,-rpath-link=$PLATFORM/usr/lib -L$PLATFORM/usr/lib -L$X264_PREFIX/lib -nostdlib -lc -lm -ldl -llog" \
    --disable-everything \
    --enable-demuxer=h264 \
    --enable-demuxer=flv \
    --enable-demuxer=mpegps \
    --enable-demuxer=mpegts \
    --enable-demuxer=mpegvideo \
    --enable-demuxer=mpegtsraw \
    --enable-demuxer=pcm_s16le \
    --enable-muxer=flv \
    --enable-muxer=pcm_s16le \
    --enable-protocol=file \
    --enable-protocol=applehttp \
    --enable-protocol=http \
    --enable-protocol=rtmp \
    --enable-protocol=rtmpt \
    --enable-protocol=rtmpe \
    --enable-protocol=rtmpte \
    --enable-protocol=rtmps \
    --enable-protocol=rtp \
    --enable-protocol=tcp \
    --enable-protocol=udp \
    --enable-avformat \
    --enable-avcodec \
    --enable-decoder=imc \
    --enable-decoder=mpeg4 \
    --enable-decoder=mpeg \
    --enable-encoder=h264 \
    --enable-decoder=h264 \
    --enable-encoder=flv \
    --enable-decoder=flv \
    --enable-decoder=adpcm_swf \
    --enable-decoder=adpcm_ms \
    --enable-decoder=adpcm_ima_wav \
    --enable-decoder=adpcm_ima_qt \
    --enable-decoder=adpcm_yamaha \
    --enable-encoder=adpcm_swf \
    --enable-encoder=adpcm_ms \
    --enable-encoder=adpcm_ima_wav \
    --enable-encoder=adpcm_ima_qt \
    --enable-encoder=adpcm_yamaha \
    --enable-encoder=pcm_s16le \
    --enable-decoder=pcm_s16le \
    --enable-decoder=pcm_s16le_planar \
    --enable-parser=h264 \
    --enable-parser=mpeg4video \
    --enable-parser=mpegaudio \
    --enable-parser=mpegvideo \
    --enable-muxer=ffm --enable-demuxer=ffm --enable-muxer=rtp --enable-demuxer=rtp \
    --enable-decoder=rtp --enable-muxer=rtsp --enable-demuxer=rtsp \
    --enable-libx264 \
    --enable-encoder=libx264 \
    $ADDITIONAL_CONFIGURE_FLAG


make clean
make  -j4 

$PREBUILT/bin/arm-linux-androideabi-ar d libavcodec/libavcodec.a inverse.o

make install


#Link all library to 1 libffmpeg.so
$PREBUILT/bin/arm-linux-androideabi-ld -rpath-link=$PLATFORM/usr/lib \
    -L$PLATFORM/usr/lib  -L$PREFIX/lib -L$X264_PREFIX/lib\
    -soname libffmpeg.so \
    -shared -nostdlib  -z,noexecstack  \
    --whole-archive --no-undefined -o $PREFIX/libffmpeg.so \
    --warn-once  --dynamic-linker=/system/bin/linker \
    $PREBUILT/lib/gcc/arm-linux-androideabi/4.4.3/libgcc.a \
    -lc -lm -lz -ldl -llog \
    -Bstatic \
    -lavcodec -lavformat \
    -lavutil -lswscale \
    -lswresample -lavfilter \
    -lavdevice -lx264 


}


function configureARMv7a 
{
echo ">>>>>>>>>> Config env for Armv7-a With NEON support <<<<<<<<<<<<<<"
#arm v7n
CPU=armv7-a
OPTIMIZE_CFLAGS="-mfloat-abi=softfp -mfpu=neon -marm -march=$CPU -mthumb -D__thumb__ "
PREFIX=./android/armeabi-v7a 
ADDITIONAL_CONFIGURE_FLAG=--enable-neon
X264_PREFIX=../x264/android/$CPU
#build_one

}

function configureARM
{
echo ">>>>>>>>>> Config env for Arm-generic Without  NEON support <<<<<<<<<<<<<<"
#arm 
CPU=armeabi
OPTIMIZE_CFLAGS=""
PREFIX=./android/$CPU 
X264_PREFIX=../x264/android/$CPU
ADDITIONAL_CONFIGURE_FLAG=


}

#arm v6
#CPU=armv6
#OPTIMIZE_CFLAGS="-marm -march=$CPU"
#PREFIX=./android/$CPU 
#ADDITIONAL_CONFIGURE_FLAG=
#build_one

#arm v7vfpv3
#CPU=armv7-a
#OPTIMIZE_CFLAGS="-mfloat-abi=softfp -mfpu=vfpv3-d16 -marm -march=$CPU "
#PREFIX=./android/$CPU 
#ADDITIONAL_CONFIGURE_FLAG=
#build_one

#arm v7vfp
#CPU=armv7-a
#OPTIMIZE_CFLAGS="-mfloat-abi=softfp -mfpu=vfp -marm -march=$CPU "
#PREFIX=./android/$CPU-vfp
#ADDITIONAL_CONFIGURE_FLAG=
#build_one


#arm v6+vfp
#CPU=armv6
#OPTIMIZE_CFLAGS="-DCMP_HAVE_VFP -mfloat-abi=softfp -mfpu=vfp -marm -march=$CPU"
#PREFIX=./android/${CPU}_vfp 
#ADDITIONAL_CONFIGURE_FLAG=
#build_one




if [ $# -lt 1 ] ; then 
#full rebuild

configureARMv7a 

build_one || exit 1

configureARM

build_one || exit 1


elif [ $1 -eq 0 ] ; then 
#- Dont reconfigure - just dirty build
echo "**************** WARNING: this will just rebuild the last configuration (arm or armv7) not both "

dirty_build
fi



