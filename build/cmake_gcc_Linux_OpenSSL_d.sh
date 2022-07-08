#!/bin/bash

SOURCE_DIR=$1

mkdir -p Linux/gcc
cd Linux/gcc

cmake -DCMAKE_BUILD_TYPE=Debug -DDEBUG_3RDPARTY_OUTPUT=ON -DCMAKE_INSTALL_PREFIX="./install" -G"Unix Makefiles" "../../${SOURCE_DIR}" -DBUILD_OPENSSL=ON -DINSTALL_3RDPARTY=OFF

cmake --build ./ --target install --config Debug

cd ../..
