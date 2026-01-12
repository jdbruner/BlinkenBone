@echo off
rem Visual Studio 2022
set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
%MSBUILD% /version

cmd /c "cd javapanelsim && ant -f build.xml compile jar"

exit /b 0

rem This can be built with VS2022, but MFC for x86 must be installed in Visual Studio
pushd blinkenlight_test\msvc
%MSBUILD%  blinkenlight_test.vcxproj
popd
