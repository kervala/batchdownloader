@echo off

:: To be sure Python and Qt are in PATH
::set PATH=%PATH%;D:\Tools\Python27;D:\Tools\Python27\Scripts;C:\Prog\Qt\6.0.0\msvc2019_64\bin
set PATH=%PATH%;D:\Tools\Python27;D:\Tools\Python27\Scripts;C:\Prog\Qt\5.15.2\msvc2019_64\bin

set CURRENTDIR=%cd%

:: Call VC++ command-line tools
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

cd %CURRENTDIR%

rmdir /s /q package_x64

mkdir package_x64

cd package_x64
cmake .. -DCMAKE_BUILD_TYPE=Release -G"NMake Makefiles"

nmake package

move *.exe ..

cd ..

pause
