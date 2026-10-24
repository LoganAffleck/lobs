#!/bin/bash
# Builds whisper.cpp 1.7.4 (speech to text for live captions) into deps/whisper (macOS;
# build-whisper.bat on Windows). Metal runs it on the GPU; the shaders are embedded, so no
# Xcode is needed. Source: deps/whisper.cpp (github.com/ggerganov/whisper.cpp, tag v1.7.4, MIT).
set -euo pipefail
cd "$(dirname "$0")"
cmake -S whisper.cpp -B whisper-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/whisper" \
	-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DBUILD_SHARED_LIBS=ON -DGGML_NATIVE=OFF -DGGML_METAL=ON \
	-DGGML_METAL_EMBED_LIBRARY=ON -DWHISPER_BUILD_EXAMPLES=OFF -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_SERVER=OFF \
	-DCMAKE_INSTALL_RPATH=@loader_path -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
cmake --build whisper-build
cmake --install whisper-build
echo WHISPER OK
