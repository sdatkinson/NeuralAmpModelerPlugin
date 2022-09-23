@echo off

REM - batch file to build Visual Studio project and zip the resulting binaries (or make installer)
REM - updating version numbers requires python and python path added to %PATH% env variable 
REM - zipping requires 7zip in %ProgramFiles%\7-Zip\7z.exe
REM - building installer requires innosetup 6 in "%ProgramFiles(x86)%\Inno Setup 6\iscc"
REM - AAX codesigning requires wraptool tool added to %PATH% env variable and aax.key/.crt in .\..\..\iPlug2\Certificates\

REM - two arguments are demo/full and zip/installer

set DEMO_ARG="%1"
set ZIP_ARG="%2"

if [%DEMO_ARG%]==[] goto USAGE
if [%ZIP_ARG%]==[] goto USAGE

echo SCRIPT VARIABLES -----------------------------------------------------
echo DEMO_ARG %DEMO_ARG% 
echo ZIP_ARG %ZIP_ARG% 
echo END SCRIPT VARIABLES -----------------------------------------------------

if %DEMO_ARG% == "demo" (
  echo Making TemplateProject Windows DEMO VERSION distribution ...
  set DEMO=1
) else (
  echo Making TemplateProject Windows FULL VERSION distribution ...
  set DEMO=0
)

if %ZIP_ARG% == "zip" (
  set ZIP=1
) else (
  set ZIP=0
)

echo ------------------------------------------------------------------
echo Updating version numbers ...

call python prepare_resources-win.py %DEMO%
call python update_installer-win.py %DEMO%

cd ..\

echo "touching source"

copy /b *.cpp+,,

echo ------------------------------------------------------------------
echo Building ...

if exist "%ProgramFiles(x86)%" (goto 64-Bit) else (goto 32-Bit)

if not defined DevEnvDir (
:32-Bit
echo 32-Bit O/S detected
call "%ProgramFiles%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_x64
goto END

:64-Bit
echo 64-Bit Host O/S detected
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_x64
goto END
:END
)


REM - set preprocessor macros like this, for instance to set demo preprocessor macro:
if %DEMO% == 1 (
  set CMDLINE_DEFINES="DEMO_VERSION=1"
  REM -copy ".\resources\img\AboutBox_Demo.png" ".\resources\img\AboutBox.png"
) else (
  set CMDLINE_DEFINES="DEMO_VERSION=0"
  REM -copy ".\resources\img\AboutBox_Registered.png" ".\resources\img\AboutBox.png"
)

REM - Could build individual targets like this:
REM - msbuild TemplateProject-app.vcxproj /p:configuration=release /p:platform=win32

REM echo Building 32 bit binaries...
REM msbuild TemplateProject.sln /p:configuration=release /p:platform=win32 /nologo /verbosity:minimal /fileLogger /m /flp:logfile=build-win.log;errorsonly 

REM echo Building 64 bit binaries...
REM add projects with /t to build VST2 and AAX
msbuild TemplateProject.sln /t:TemplateProject-app;TemplateProject-vst3 /p:configuration=release /p:platform=x64 /nologo /verbosity:minimal /fileLogger /m /flp:logfile=build-win.log;errorsonly;append

REM --echo Copying AAX Presets

REM --echo ------------------------------------------------------------------
REM --echo Code sign AAX binary...
REM --info at pace central, login via iLok license manager https://www.paceap.com/pace-central.html
REM --wraptool sign --verbose --account XXXXX --wcguid XXXXX --keyfile XXXXX.p12 --keypassword XXXXX --in .\build-win\aax\bin\TemplateProject.aaxplugin\Contents\Win32\TemplateProject.aaxplugin --out .\build-win\aax\bin\TemplateProject.aaxplugin\Contents\Win32\TemplateProject.aaxplugin
REM --wraptool sign --verbose --account XXXXX --wcguid XXXXX --keyfile XXXXX.p12 --keypassword XXXXX --in .\build-win\aax\bin\TemplateProject.aaxplugin\Contents\x64\TemplateProject.aaxplugin --out .\build-win\aax\bin\TemplateProject.aaxplugin\Contents\x64\TemplateProject.aaxplugin

if %ZIP% == 0 (
REM - Make Installer (InnoSetup)

echo ------------------------------------------------------------------
echo Making Installer ...

  REM if exist "%ProgramFiles(x86)%" (goto 64-Bit-is) else (goto 32-Bit-is)

  REM :32-Bit-is
  REM REM "%ProgramFiles%\Inno Setup 6\iscc" /Q ".\installer\TemplateProject.iss"
  REM goto END-is

  REM :64-Bit-is
  "%ProgramFiles(x86)%\Inno Setup 6\iscc" /Q ".\installer\TemplateProject.iss"
  REM goto END-is

  REM :END-is

  REM - Codesign Installer for Windows 8+
  REM -"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Bin\signtool.exe" sign /f "XXXXX.p12" /p XXXXX /d "TemplateProject Installer" ".\installer\TemplateProject Installer.exe"

  REM -if %1 == 1 (
  REM -copy ".\installer\TemplateProject Installer.exe" ".\installer\TemplateProject Demo Installer.exe"
  REM -del ".\installer\TemplateProject Installer.exe"
  REM -)

  echo Making Zip File of Installer ...
) else (
  echo Making Zip File ...
)

FOR /F "tokens=* USEBACKQ" %%F IN (`call python scripts\makezip-win.py %DEMO% %ZIP%`) DO (
SET ZIP_NAME=%%F
)

echo ------------------------------------------------------------------
echo Printing log file to console...

type build-win.log
goto SUCCESS

:USAGE
echo Usage: %0 [demo/full] [zip/installer]
exit /B 1

:SUCCESS
echo %ZIP_NAME%

exit /B 0