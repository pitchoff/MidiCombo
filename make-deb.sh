#!/bin/sh
# Builds a Debian package of MidiCombo into bin/.
# Needs cmake, g++, qt6-base-dev, libasound2-dev, pkg-config and dpkg-dev.
set -e
cd "$(dirname "$0")"

cmake -S . -B build-deb -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-deb -j"$(nproc)"
(cd build-deb && ctest --output-on-failure)

mkdir -p bin
(cd build-deb && cpack -G DEB -B "$PWD/../bin")
rm -rf bin/_CPack_Packages

ls -l bin/*.deb
