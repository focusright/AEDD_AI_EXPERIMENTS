@echo off
setlocal
cd /d "%~dp0"

if not exist "third_party\imgui\imgui.cpp" (
  echo Fetching Dear ImGui...
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\FetchImGui.ps1"
  if errorlevel 1 exit /b 1
)

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do set VSINSTALL=%%i
if not defined VSINSTALL (
  echo Visual Studio 2022 not found.
  exit /b 1
)

call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=amd64
msbuild AERIGP1.sln /p:Configuration=Debug /p:Platform=x64 /m
exit /b %ERRORLEVEL%
