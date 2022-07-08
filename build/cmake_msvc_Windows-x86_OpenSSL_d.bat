@echo off

set SOURCE_DIR=%1

md "Windows/msvc-x86"
cd "Windows/msvc-x86"

cmake -DDEBUG_3RDPARTY_OUTPUT=ON -DCMAKE_INSTALL_PREFIX="./install" -G"Visual Studio 16 2019" -A Win32 -T ClangCL "../../%SOURCE_DIR%" -DBUILD_OPENSSL=ON -DINSTALL_3RDPARTY=ON

cmake --build ./ --target install --config Debug

cd ../..