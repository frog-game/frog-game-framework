@echo off

set CURRENT_DIR=%cd%

set FIND_DIR=%CURRENT_DIR%\..\..\..\..\src
echo %FIND_DIR%
for /r %FIND_DIR% %%i in (*.c,*cpp,*hpp,*h) do ( clang-format.exe -i %%i -style=file )
pause
