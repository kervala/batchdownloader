@echo off

set COMPILER=vc12
set UNIXDIR=D:\Projects\packaging\unix
set CURRENTDIR=%cd%

set QTVERSION=5.10.1

cd %UNIXDIR%
call setenv.bat %COMPILER% amd64

cd %CURRENTDIR%

set QTDIR=%WIN32CROSS_PATH%/%COMPILER%/external/qt-%QTVERSION%-static-64

rmdir /s /q package_x64

mkdir package_x64

cd package_x64
cmake .. -DCMAKE_BUILD_TYPE=Release -G"NMake Makefiles"

nmake package

move *.exe ..

cd ..

pause
