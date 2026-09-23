@echo off
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
"H:\ProgramFiles\QT\6.8.3\Tools\Ninja\ninja.exe" -C D:\code\QT\QGroundControl\qgroundcontrol\build\Desktop_Qt_6_8_3_MSVC2022_64bit-Debug -j 3 DGCS
exit /b %errorlevel%
