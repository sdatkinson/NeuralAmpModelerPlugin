@echo off

REM - CALL "$(SolutionDir)scripts\postbuild-win.bat" "$(TargetExt)" "$(BINARY_NAME)" "$(Platform)" "$(COPY_VST2)" "$(TargetPath)" "$(VST2_32_PATH)" "$(VST2_64_PATH)" "$(VST3_32_PATH)" "$(VST3_64_PATH)" "$(AAX_32_PATH)" "$(AAX_64_PATH)" "$(BUILD_DIR)" "$(VST_ICON)" "$(AAX_ICON)" "
REM $(CREATE_BUNDLE_SCRIPT)"

set FORMAT=%1
set NAME=%2
set PLATFORM=%3
set COPY_VST2=%4
set BUILT_BINARY=%5
set VST2_32_PATH=%6
set VST2_64_PATH=%7 
set VST3_32_PATH=%8
set VST3_64_PATH=%9
shift
shift 
shift
shift
shift 
shift
set AAX_32_PATH=%4
set AAX_64_PATH=%5
set BUILD_DIR=%6
set VST_ICON=%7
set AAX_ICON=%8
set CREATE_BUNDLE_SCRIPT=%9

echo POSTBUILD SCRIPT VARIABLES -----------------------------------------------------
echo FORMAT %FORMAT% 
echo NAME %NAME% 
echo PLATFORM %PLATFORM% 
echo COPY_VST2 %COPY_VST2% 
echo BUILT_BINARY %BUILT_BINARY% 
echo VST2_32_PATH %VST2_32_PATH% 
echo VST2_64_PATH %VST2_64_PATH% 
echo VST3_32_PATH %VST3_32_PATH% 
echo VST3_64_PATH %VST3_64_PATH% 
echo BUILD_DIR %BUILD_DIR%
echo VST_ICON %VST_ICON% 
echo AAX_ICON %AAX_ICON% 
echo CREATE_BUNDLE_SCRIPT %CREATE_BUNDLE_SCRIPT%
echo END POSTBUILD SCRIPT VARIABLES -----------------------------------------------------

if %PLATFORM% == "Win32" (
  if %FORMAT% == ".exe" (
    copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%_%PLATFORM%.exe
  )

  if %FORMAT% == ".dll" (
    copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%_%PLATFORM%.dll
  )
  
  if %FORMAT% == ".dll" (
    if %COPY_VST2% == "1" (
      echo copying 32bit binary to 32bit VST2 Plugins folder ... 
      copy /y %BUILT_BINARY% %VST2_32_PATH%
    ) else (
      echo not copying 32bit VST2 binary
    )
  )
  
  if %FORMAT% == ".vst3" (
    echo copying 32bit binary to VST3 BUNDLE ..
    call %CREATE_BUNDLE_SCRIPT% %BUILD_DIR%\%NAME%.vst3 %VST_ICON% %FORMAT%
    copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%.vst3\Contents\x86-win
    if exist %VST3_32_PATH% ( 
      echo copying VST3 bundle to 32bit VST3 Plugins folder ...
      call %CREATE_BUNDLE_SCRIPT% %VST3_32_PATH%\%NAME%.vst3 %VST_ICON% %FORMAT%
      xcopy /E /H /Y %BUILD_DIR%\%NAME%.vst3\Contents\*  %VST3_32_PATH%\%NAME%.vst3\Contents\
    )
  )
  
  if %FORMAT% == ".aaxplugin" (
    echo copying 32bit binary to AAX BUNDLE ..
    call %CREATE_BUNDLE_SCRIPT% %BUILD_DIR%\%NAME%.aaxplugin %AAX_ICON% %FORMAT%
    copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%.aaxplugin\Contents\Win32
    echo copying 32bit bundle to 32bit AAX Plugins folder ... 
    call %CREATE_BUNDLE_SCRIPT% %BUILD_DIR%\%NAME%.aaxplugin %AAX_ICON% %FORMAT%
    xcopy /E /H /Y %BUILD_DIR%\%NAME%.aaxplugin\Contents\* %AAX_32_PATH%\%NAME%.aaxplugin\Contents\
  )
)

if %PLATFORM% == "x64" (
  if not exist "%ProgramFiles(x86)%" (
    echo "This batch script fails on 32 bit windows... edit accordingly"
  )

  if %FORMAT% == ".exe" (
    copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%_%PLATFORM%.exe
  )

  if %FORMAT% == ".dll" (
    copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%_%PLATFORM%.dll
  )
  
  if %FORMAT% == ".dll" (
    if %COPY_VST2% == "1" (
      echo copying 64bit binary to 64bit VST2 Plugins folder ... 
      copy /y %BUILT_BINARY% %VST2_64_PATH%
    ) else (
      echo not copying 64bit VST2 binary
    )
  )
  
  if %FORMAT% == ".vst3" (
    echo copying 64bit binary to VST3 BUNDLE ...
    call %CREATE_BUNDLE_SCRIPT% %BUILD_DIR%\%NAME%.vst3 %VST_ICON% %FORMAT%
    copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%.vst3\Contents\x86_64-win
    if exist %VST3_64_PATH% (
      echo copying VST3 bundle to 64bit VST3 Plugins folder ...
      call %CREATE_BUNDLE_SCRIPT% %VST3_64_PATH%\%NAME%.vst3 %VST_ICON% %FORMAT%
      xcopy /E /H /Y %BUILD_DIR%\%NAME%.vst3\Contents\*  %VST3_64_PATH%\%NAME%.vst3\Contents\
    )
  )
  
  if %FORMAT% == ".aaxplugin" (
    echo copying 64bit binary to AAX BUNDLE ...
    call %CREATE_BUNDLE_SCRIPT% %BUILD_DIR%\%NAME%.aaxplugin %AAX_ICON% %FORMAT%
    copy /y %BUILT_BINARY% %BUILD_DIR%\%NAME%.aaxplugin\Contents\x64
    echo copying 64bit bundle to 64bit AAX Plugins folder ... 
    call %CREATE_BUNDLE_SCRIPT% %BUILD_DIR%\%NAME%.aaxplugin %AAX_ICON% %FORMAT%
    xcopy /E /H /Y %BUILD_DIR%\%NAME%.aaxplugin\Contents\* %AAX_64_PATH%\%NAME%.aaxplugin\Contents\
  )
)