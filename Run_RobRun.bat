@echo off
setlocal
title RobRun Overlay - one click
cd /d "%~dp0"

rem 1) Prebuilt exe next to this file (CI artifact / release zip)
if exist "RobRunOverlay.exe" (
  start "" "RobRunOverlay.exe" %*
  goto :eof
)

rem 2) Already built from source
if exist "build\Release\RobRunOverlay.exe" (
  start "" "build\Release\RobRunOverlay.exe" %*
  goto :eof
)

rem 3) Build from source (needs Visual Studio 2022 C++ tools + CMake + internet for Dear ImGui)
where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake was not found and no prebuilt RobRunOverlay.exe is present.
  echo Either download the "RobRunOverlay-*" artifact from GitHub Actions,
  echo or install Visual Studio 2022 ^(Desktop development with C++^) and CMake.
  pause
  exit /b 1
)

echo Configuring...
cmake -B build -S . -A x64 || goto :fail
echo Building (Release)...
cmake --build build --config Release --parallel || goto :fail
echo Running tests...
ctest --test-dir build -C Release --output-on-failure || goto :fail

start "" "build\Release\RobRunOverlay.exe" %*
goto :eof

:fail
echo.
echo Build or tests FAILED - see the messages above. The app was not started.
pause
exit /b 1
