@echo off

set SOURCE_DIR=%1

md "Windows/msvc-x64"
cd "Windows/msvc-x64"

cmake -DCMAKE_INSTALL_PREFIX="./install" -G"Visual Studio 16 2019" -A x64 -T ClangCL "../../%SOURCE_DIR%" -DINSTALL_3RDPARTY=ON

cmake --build ./ --target install --config Release

cd ../..
