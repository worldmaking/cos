@echo off
@setlocal
rem 64-bit version
rem first argument is the filename (minus the .cpp) to compile
rem e.g. dyn_make64.bat foo will try to compile foo.cpp into foo.x64.dll
IF [%1]==[] ( SET "INNAME=dyn_gl" ) ELSE ( SET "INNAME=%1" )
SET OUTNAME=%INNAME%.x64

rem look for max-sdk here:
if exist "%userprofile%\\Documents\\Max 7\\Packages\\max-sdk" (SET MAXSDK=%userprofile%\\Documents\\Max 7\\Packages\\max-sdk)
if exist "%userprofile%\\Documents\\Max 8\\Packages\\max-sdk" (SET MAXSDK=%userprofile%\\Documents\\Max 8\\Packages\\max-sdk)

rem assume libraries etc. are found relative to current directory:
SET VSCMD_START_DIR=%cd%

rem set up environment for 64-bit compiling:
SET VCVARS64=VC\Auxiliary\Build\vcvars64.bat
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Enterprise\%VCVARS64%" call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Enterprise\%VCVARS64%"
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Professional\%VCVARS64%" call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Professional\%VCVARS64%"
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\%VCVARS64%" call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\%VCVARS64%"
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\BuildTools\%VCVARS64%" call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\BuildTools\%VCVARS64%"

echo INPUT: %INNAME%.cpp
echo OUTPUT: %OUTNAME%.x64.dll
echo MAXSDK: %MAXSDK%

rem compile it
cl /nologo /EHsc /D "WIN_VERSION" /D "WIN64" /D "NDEBUG" /O2 "%INNAME%.cpp" /Fe: "%OUTNAME%.dll" /I "%cd%\\..\\include" /I "%cd%\\..\\include\\al" /I "%MAXSDK%\\source\\c74support\\max-includes\\" /I "%MAXSDK%\\source\\c74support\\msp-includes\\" /link /DLL /MACHINE:X64 "%MAXSDK%\\source\\c74support\\max-includes\\x64\\MaxAPI.lib" "%MAXSDK%\\source\\c74support\\msp-includes\\x64\\MaxAudio.lib" /WHOLEARCHIVE:"%cd%\\..\\lib\\win64\\lib-vc2017\\glfw3.lib" user32.lib kernel32.lib shell32.lib shlwapi.lib gdi32.lib advapi32.lib iphlpapi.lib ole32.lib oleaut32.lib odbc32.lib odbccp32.lib psapi.lib userenv.lib uuid.lib winspool.lib comdlg32.lib ws2_32.lib opengl32.lib

rem cl /nologo /EHsc /D "WIN_VERSION" /D "WIN64" /D "NDEBUG" /O2 "%INNAME%.cpp" /Fe: "%OUTNAME%.dll" /I "%cd%\\..\\include" /I "%cd%\\..\\include\\al" /I "%MAXSDK%\\source\\c74support\\max-includes\\" /I "%MAXSDK%\\source\\c74support\\msp-includes\\" /I "%MAXSDK%\\source\\c74support\\jit-includes\\" /link /DLL /MACHINE:X64 "%MAXSDK%\\source\\c74support\\max-includes\\x64\\MaxAPI.lib" "%MAXSDK%\\source\\c74support\\msp-includes\\x64\\MaxAudio.lib" "%MAXSDK%\\source\\c74support\\jit-includes\\x64\\jitlib.lib" /WHOLEARCHIVE:"%cd%\\..\\lib\\win64\\lib-vc2017\\glfw3.lib" user32.lib kernel32.lib shell32.lib shlwapi.lib gdi32.lib advapi32.lib iphlpapi.lib ole32.lib oleaut32.lib odbc32.lib odbccp32.lib psapi.lib userenv.lib uuid.lib winspool.lib comdlg32.lib ws2_32.lib opengl32.lib

rem couldn't find a way to stop cl.exe from generating these, so clean up after
del "%INNAME%.obj" "%INNAME%.x64.exp" "%INNAME%.lib"

echo ok