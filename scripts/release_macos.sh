#!/bin/bash

if [ "$#" -ne 1 ]; then
  echo "Usage: ./scripts/release_macos.sh path-to-bin-folder"
  exit 1
fi

realpath() {
  OURPWD=$PWD
  cd "$(dirname "$1")"
  LINK=$(readlink "$(basename "$1")")
  while [ "$LINK" ]; do
    cd "$(dirname "$LINK")"
    LINK=$(readlink "$(basename "$1")")
  done
  destdir="$PWD/$(basename "$1")"
  cd "$OURPWD"
}

realpath "$1"

mkdir -p build
cd build
rm CMakeCache.txt # make sure to use default settings
cmake \
-D COMPILER=ICC \
-D CMAKE_SKIP_INSTALL_RPATH=ON \
..

# make docu after cmake to have correct version.h
# assumes documentation repo cloned into embree-doc
make -C ../embree-doc docbin

make -j 8 preinstall
umask_org=`umask` # workaround for bug in CMake/CPack: need to reset umask
umask 022
cmake -D CMAKE_INSTALL_PREFIX="$destdir" -P cmake_install.cmake
umask $umask_org
cd ..

# install scripts
install scripts/install_macos/paths.sh "$destdir"
sed -e "s/@EMBREE_VERSION@/`cat embree-doc/version`/" scripts/install_macos/install.sh > "$destdir"/install.sh
chmod 755 "$destdir"/install.sh
