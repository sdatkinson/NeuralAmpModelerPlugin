@echo off
setlocal  EnableExtensions EnableDelayedExpansion

echo Generating visual studio 2022 test project in ..\build-win. cmake must be in your PATH

set CMKDEF="C:\Program Files\CMake\bin\cmake.exe"
set CMKCMD=cmake.exe

if exist %CMKDEF% (
  set CMK=%CMKDEF%
) else (
  set CMK=%CMKCMD%
)

%CMK%  -G "Visual Studio 17 2022" -S . -B..\build-win\test