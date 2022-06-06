@echo off
set YEAR=2019
set EDITION=Community
set VSROOT=%ProgramFiles(x86)%\Microsoft Visual Studio\%YEAR%\%EDITION%\VC\Auxiliary\Build
set SUBSYSTEM=WINDOWS
set DEBUG=0

cd %~dp0
if not exist "bin" mkdir bin

set LIBS=User32.lib Gdi32.lib
set DBG=
if %DEBUG% NEQ 0 set DBG=/DEBUG:FULL

cmd /c "call "%VSROOT%\vcvars32.bat" & cl main.c %LIBS% /Fo:bin\giant_mouse32.obj /Fe:bin\giant_cursor32.exe" /link /SUBSYSTEM:%SUBSYSTEM% %DBG%

cmd /c "call "%VSROOT%\vcvarsx86_amd64.bat" & cl main.c %LIBS% /Fo:bin\giant_mouse64.obj /Fe:bin\giant_cursor64.exe" /link /SUBSYSTEM:%SUBSYSTEM% %DBG%
