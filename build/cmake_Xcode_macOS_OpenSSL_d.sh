#!/bin/bash

SOURCE_DIR=$1

mkdir -p macOS/XCode
cd macOS/XCode

cmake -DDEBUG_3RDPARTY_OUTPUT=ON -DCMAKE_INSTALL_PREFIX="./install" -GXcode "../../${SOURCE_DIR}" -DBUILD_OPENSSL=ON -DINSTALL_3RDPARTY=ON

cmake --build ./ --target install --config Debug

cd ../..
