#!/bin/sh
# Run in w64devkit sh from the project root. Source archives are in work/packaging.
set -eu
base=$(pwd)
src="$base/work/codec-src"
prefix="$base/work/codec-prefix"
mkdir -p "$prefix"
mkdir -p "$base/work/codec-temp"
export TMPDIR="$base/work/codec-temp"
export TEMP="$TMPDIR"
export TMP="$TMPDIR"
export CFLAGS='-O2 -std=gnu11'
export CXXFLAGS='-O2'
export PATH_SEPARATOR=';'
export ac_executable_extensions=.exe
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
jobs=${VAMP_BUILD_JOBS:-4}
cd "$src/lame-3.100"
if [ ! -f Makefile ]; then
  ./configure --build=x86_64-w64-mingw32 --host=x86_64-w64-mingw32 --prefix="$prefix" --disable-shared --enable-static --disable-frontend --disable-nasm >configure-vamp.log 2>&1
fi
make -j"$jobs" >build-vamp.log 2>&1
make install >install-vamp.log 2>&1
for name in libogg-1.3.6 libvorbis-1.3.7 opus-1.5.2; do
  cmake -S "$src/$name" -B "$src/$name/vamp-build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_PREFIX_PATH="$prefix" -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_TESTING=OFF -DINSTALL_DOCS=OFF -DOPUS_BUILD_TESTING=OFF \
    -DOPUS_BUILD_PROGRAMS=OFF >"$src/$name/configure-vamp.log" 2>&1
  cmake --build "$src/$name/vamp-build" -j "$jobs" >"$src/$name/build-vamp.log" 2>&1
  cmake --install "$src/$name/vamp-build" >"$src/$name/install-vamp.log" 2>&1
done
cd "$src/ffmpeg-8.1.3"
if [ ! -f ffbuild/config.mak ]; then
 ./configure --prefix="$prefix" --cc=gcc --cxx=g++ --ar=ar --nm=nm --ranlib=ranlib \
  --target-os=mingw32 --arch=x86_64 --disable-x86asm --disable-debug --disable-doc \
  --disable-autodetect --disable-network --disable-everything --disable-ffplay --disable-ffprobe \
  --enable-small --enable-static --disable-shared --extra-ldflags=-static \
  --extra-cflags="-I$prefix/include" --extra-ldflags="-static -L$prefix/lib" \
  --pkg-config=pkg-config --pkg-config-flags=--static \
  --enable-libmp3lame --enable-libvorbis --enable-libopus \
  --enable-protocol=file,pipe --enable-filter=aresample,aformat,anull \
  --enable-demuxer=wav,mp3,flac,ogg,mov,aiff \
  --enable-muxer=wav,mp3,flac,ogg,opus,ipod,mov,aiff,adts \
  --enable-parser=mpegaudio,flac,vorbis,opus,aac \
  --enable-decoder=pcm_s16le,pcm_s24le,pcm_f32le,pcm_f64le,pcm_s16be,pcm_s24be,mp3,flac,vorbis,opus,aac \
  --enable-encoder=pcm_s16le,pcm_s24le,pcm_f32le,pcm_s16be,pcm_s24be,flac,aac,libmp3lame,libvorbis,libopus \
  >configure-vamp.log 2>&1
fi
make -j"$jobs" >build-vamp.log 2>&1
make install >install-vamp.log 2>&1
"$prefix/bin/ffmpeg.exe" -version
