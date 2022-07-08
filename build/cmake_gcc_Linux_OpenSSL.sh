#!/bin/bash

SOURCE_DIR=$1

mkdir -p Linux/gcc
cd Linux/gcc

cmake -DCMAKE_INSTALL_PREFIX="./install" -G"Unix Makefiles"  "../../${SOURCE_DIR}" -DBUILD_OPENSSL=ON -DINSTALL_3RDPARTY=ON

cmake --build ./ --target install --config Release

cd ../..
