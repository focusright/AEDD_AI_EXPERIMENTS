@echo off
setlocal
set PATH=
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
msbuild HeatMethodDx12Demo.sln /m /p:Configuration=Release /p:Platform=x64
