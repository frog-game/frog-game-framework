# Frog
lua c event-driven framework

## build for Windows
=================================
### MSVC build

Install Visual Studio 2019 with  Clang compiler tools for Windows.
    For best IDE support in Visual Studio, we recommend using the latest Clang compiler tools for Windows. 
    If you don't already have those, you can install them by opening the Visual Studio Installer and choosing Clang compiler for Windows under Desktop development with C++ optional components.

Install CMake

#### use cmake_msvc_Windows-x86.bat or cmake_msvc_Windows-x64.bat build
Install OpenSSL

Use VS2019 x86/x64 Native Tools Command: cd build, run cmake_msvc_Windows-x86.bat path(Frog/) or cmake_msvc_Windows-x64.bat path(Frog/). 

#### use cmake_msvc_Windows-x86_OpenSSL.bat or cmake_msvc_Windows-x64_OpenSSL.bat build
Install OpenSSL build dependent
    - Perl. We recommend ActiveState Perl, available from
    https://www.activestate.com/ActivePerl. Another viable alternative
    appears to be Strawberry Perl, http://strawberryperl.com.
    - Install nasm. Add bin\NASM to the PATH environment variable. 
    http://www.nasm.us/pub/nasm/releasebuilds/

Use VS2019 x86/x64 Native Tools Command: cd build, run cmake_msvc_Windows-x86_OpenSSL.bat path(Frog/) or cmake_msvc_Windows-x64_OpenSSL.bat path(Frog/). 

## build for MacOS
=================================
### Xcode

Install Xcode

Install CMake

#### use cmake_Xcode_macOS.sh build
Install OpenSSL

Use Command Line Tools: cd build, run sh cmake_Xcode_macOS.sh path(Frog/).  

#### use cmake_Xcode_macOS_OpenSSL.sh build
Use Command Line Tools: cd build, run sh cmake_Xcode_macOS_OpenSSL.sh path(Frog/).  

## build for Linux
=================================
### gcc

Install gcc

Install CMake

#### use cmake_gcc_Linux.sh build
Install OpenSSL

Use Command Line Tools: cd build, run sh cmake_gcc_Linux.sh path(Frog/). 

#### use cmake_gcc_Linux_OpenSSL.sh build
Use Command Line Tools: cd build, run sh cmake_gcc_Linux_OpenSSL.sh path(Frog/). 
