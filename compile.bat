@echo off
cd %~dp0
mkdir build
pushd build
rem set compiler_flags=/I "..\source" -MT -GR- -nologo -Od -WX -W4 -wd4800 -wd4996 -wd4100 -wd4200 -wd4189 -wd4055 -wd4054 -Z7 -Ddebug -EHsc -utf-8 
set compiler_flags=/I "..\source" -nologo -Od -Z7 -W4 -Ddebug -utf-8 

cl %compiler_flags%  ../source/test.cpp

cl %compiler_flags% ../source/tcb_converter.cpp

popd