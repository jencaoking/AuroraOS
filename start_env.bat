@echo off
:: auroraOS Environment Setup
:: You can set these environment variables before running this script,
:: otherwise they will default to the J:\CODE paths used by the original author.

if "%MY_CMAKE_PATH%"=="" set "MY_CMAKE_PATH=J:\CODE\CMAKE\bin"
if "%MY_MINGW_PATH%"=="" set "MY_MINGW_PATH=J:\CODE\MINGW64\bin"
if "%MY_QEMU_PATH%"=="" set "MY_QEMU_PATH=J:\CODE\QEMU"
if "%MY_ARM_PATH%"==""  set "MY_ARM_PATH=J:\CODE\arm-gnu-toolchain\bin"

set "PATH=%MY_ARM_PATH%;%MY_CMAKE_PATH%;%MY_MINGW_PATH%;%MY_QEMU_PATH%;%PATH%"

echo ====================================================
echo  auroraOS Local Dev Environment Activated
echo  CMake:       %MY_CMAKE_PATH%
echo  MinGW64:     %MY_MINGW_PATH%
echo  QEMU:        %MY_QEMU_PATH%
echo  ARM Tool:    %MY_ARM_PATH%
echo ====================================================
echo.

cmd /k
