@ECHO off

set EXE=giant_cursor64.exe
IF "%ProgramW6432%"=="" set EXE=giant_cursor32.exe

cd %~dp0
start %EXE%
