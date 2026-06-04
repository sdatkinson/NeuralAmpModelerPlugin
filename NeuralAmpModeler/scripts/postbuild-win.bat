@echo off

REM - CALL "$(SolutionDir)scripts\postbuild-win.bat" "$(TargetExt)" "$(BINARY_NAME)" "$(Platform)" "$(TargetPath)" "$(VST3_32_PATH)" "$(VST3_64_PATH)" "$(AAX_32_PATH)" "$(AAX_64_PATH)" "$(BUILD_DIR)" "$(VST_ICON)" "$(AAX_ICON)" "
REM $(CREATE_BUNDLE_SCRIPT)"

REM Anchor paths to this script: if %0 is relative, %~dp0 can be wrong; %~f0 fixes that.
for %%I in ("%~f0") do set "POSTBUILD_SCRIPT_DIR=%%~dpI"
set "THIRD_PARTY_NOTICES=%POSTBUILD_SCRIPT_DIR%..\installer\ThirdPartyNotices.txt"

REM MSBuild may quote args; SET uses tilde+param to strip quotes so FORMAT matches .vst3.
set "FORMAT=%~1"
set "NAME=%~2"
set "PLATFORM=%~3"
set "BUILT_BINARY=%~4"
set "BUILT_BINARY_DIR=%~dp4"
set "VST3_32_PATH=%~5"
set "VST3_64_PATH=%~6"
shift
shift
shift
shift
shift
shift
set "AAX_32_PATH=%~1"
set "AAX_64_PATH=%~2"
set "BUILD_DIR=%~3"
set "VST_ICON=%~4"
set "AAX_ICON=%~5"
set "CREATE_BUNDLE_SCRIPT=%~6"

REM Paths like Program Files (x86)\... contain "(". Inside (...) blocks, %VAR% expands at parse time and breaks IF parsing — use !VAR!.
setlocal EnableDelayedExpansion

echo POSTBUILD SCRIPT VARIABLES -----------------------------------------------------
echo FORMAT %FORMAT%
echo NAME %NAME%
echo PLATFORM %PLATFORM%
echo BUILT_BINARY %BUILT_BINARY%
echo VST3_32_PATH %VST3_32_PATH%
echo VST3_64_PATH %VST3_64_PATH%
echo BUILD_DIR %BUILD_DIR%
echo VST_ICON %VST_ICON%
echo AAX_ICON %AAX_ICON%
echo CREATE_BUNDLE_SCRIPT %CREATE_BUNDLE_SCRIPT%
echo END POSTBUILD SCRIPT VARIABLES -----------------------------------------------------

if not exist "%THIRD_PARTY_NOTICES%" (
  echo ThirdPartyNotices.txt not found at "%THIRD_PARTY_NOTICES%"
)

REM Use quoted IF operands: "if x64 == \"x64\"" is false in cmd.exe; "if \"%VAR%\"==\"x64\"" works.
if /i "%PLATFORM%"=="Win32" (
  if "%FORMAT%"==".exe" (
    copy /y "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%_%PLATFORM%.exe"
    call :CopyThirdPartyNotices "%BUILD_DIR%"
    call :CopyThirdPartyNotices "%BUILT_BINARY_DIR%"
  )

  if "%FORMAT%"==".dll" (
    copy /y "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%_%PLATFORM%.dll"
  )

  if "%FORMAT%"==".vst3" (
    echo copying 32bit binary to VST3 BUNDLE ..
    call "%CREATE_BUNDLE_SCRIPT%" "%BUILD_DIR%\%NAME%.vst3" "%VST_ICON%" "%FORMAT%"
    copy /y "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%.vst3\Contents\x86-win"
    call :CopyThirdPartyNotices "%BUILD_DIR%\%NAME%.vst3\Contents\Resources"
    call :CopyThirdPartyNotices "%BUILT_BINARY_DIR%"
    if exist "!VST3_32_PATH!" (
      echo copying VST3 bundle to 32bit VST3 Plugins folder ...
      call "%CREATE_BUNDLE_SCRIPT%" "!VST3_32_PATH!\%NAME%.vst3" "%VST_ICON%" "%FORMAT%"
      xcopy /E /H /Y "%BUILD_DIR%\%NAME%.vst3\Contents\*" "!VST3_32_PATH!\%NAME%.vst3\Contents\"
    )
  )

  if "%FORMAT%"==".aaxplugin" (
    echo copying 32bit binary to AAX BUNDLE ..
    call "%CREATE_BUNDLE_SCRIPT%" "%BUILD_DIR%\%NAME%.aaxplugin" "%AAX_ICON%" "%FORMAT%"
    copy /y "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%.aaxplugin\Contents\Win32"
    call :CopyThirdPartyNotices "%BUILD_DIR%\%NAME%.aaxplugin\Contents\Resources"
    echo copying 32bit bundle to 32bit AAX Plugins folder ...
    call "%CREATE_BUNDLE_SCRIPT%" "%BUILD_DIR%\%NAME%.aaxplugin" "%AAX_ICON%" "%FORMAT%"
    xcopy /E /H /Y "%BUILD_DIR%\%NAME%.aaxplugin\Contents\*" "!AAX_32_PATH!\%NAME%.aaxplugin\Contents\"
  )
)

if /i "%PLATFORM%"=="x64" (
  if "%FORMAT%"==".exe" (
    copy /y "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%_%PLATFORM%.exe"
    call :CopyThirdPartyNotices "%BUILD_DIR%"
    call :CopyThirdPartyNotices "%BUILT_BINARY_DIR%"
  )

  if "%FORMAT%"==".dll" (
    copy /y "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%_%PLATFORM%.dll"
  )

  if "%FORMAT%"==".vst3" (
    echo copying 64bit binary to VST3 BUNDLE ...
    call "%CREATE_BUNDLE_SCRIPT%" "%BUILD_DIR%\%NAME%.vst3" "%VST_ICON%" "%FORMAT%"
    copy /y "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%.vst3\Contents\x86_64-win"
    call :CopyThirdPartyNotices "%BUILD_DIR%\%NAME%.vst3\Contents\Resources"
    call :CopyThirdPartyNotices "%BUILT_BINARY_DIR%"
    if exist "!VST3_64_PATH!" (
      echo copying VST3 bundle to 64bit VST3 Plugins folder ...
      call "%CREATE_BUNDLE_SCRIPT%" "!VST3_64_PATH!\%NAME%.vst3" "%VST_ICON%" "%FORMAT%"
      xcopy /E /H /Y "%BUILD_DIR%\%NAME%.vst3\Contents\*" "!VST3_64_PATH!\%NAME%.vst3\Contents\"
    )
  )

  if "%FORMAT%"==".aaxplugin" (
    echo copying 64bit binary to AAX BUNDLE ...
    call "%CREATE_BUNDLE_SCRIPT%" "%BUILD_DIR%\%NAME%.aaxplugin" "%AAX_ICON%" "%FORMAT%"
    copy /y "%BUILT_BINARY%" "%BUILD_DIR%\%NAME%.aaxplugin\Contents\x64"
    call :CopyThirdPartyNotices "%BUILD_DIR%\%NAME%.aaxplugin\Contents\Resources"
    echo copying 64bit bundle to 64bit AAX Plugins folder ...
    call "%CREATE_BUNDLE_SCRIPT%" "%BUILD_DIR%\%NAME%.aaxplugin" "%AAX_ICON%" "%FORMAT%"
    xcopy /E /H /Y "%BUILD_DIR%\%NAME%.aaxplugin\Contents\*" "!AAX_64_PATH!\%NAME%.aaxplugin\Contents\"
  )
)

goto :eof

:CopyThirdPartyNotices
if exist "%THIRD_PARTY_NOTICES%" (
  if not exist "%~1" mkdir "%~1"
  copy /y "%THIRD_PARTY_NOTICES%" "%~1\ThirdPartyNotices.txt"
)
goto :eof
