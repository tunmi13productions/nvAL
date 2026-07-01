@echo off
:: nvAL build script for Windows (MSVC)
:: Usage: build.bat [path\to\nvgt]
:: Or set NVGT_PATH environment variable before running.

if not "%~1"=="" set NVGT_PATH=%~1
if "%NVGT_PATH%"=="" (
    echo Error: NVGT_PATH not set.
    echo Usage: build.bat C:\path\to\nvgt
    echo   or:  set NVGT_PATH=C:\path\to\nvgt ^&^& build.bat
    exit /b 1
)

set SRC=%~dp0..\src
set OUT=%~dp0..\nval.dll
set INC_NVGT=%NVGT_PATH%\src
:: angelscript.h ships in the platform dev bundle (windev\include). CI can pre-set
:: INC_AS to a directory holding just angelscript.h to avoid unpacking the whole bundle.
if "%INC_AS%"=="" set INC_AS=%NVGT_PATH%\windev\include
set INC_ASADDON=%NVGT_PATH%\ASAddon\include
set SCRIPTARRAY=%NVGT_PATH%\ASAddon\plugin\scriptarray.cpp

:: Find MSVC via vswhere
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% (
    echo Error: vswhere.exe not found. Is Visual Studio Build Tools installed?
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS_PATH=%%i
set VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat

echo Building nvAL...
call "%VCVARS%" > nul 2>&1
cl /LD /EHsc /std:c++17 /I"%INC_NVGT%" /I"%INC_AS%" /I"%INC_ASADDON%" /I"%SRC%" "%SRC%\nval.cpp" "%SCRIPTARRAY%" /link /OUT:"%OUT%"
if %errorlevel%==0 (
    echo.
    echo Success: nval.dll written to repo root.
) else (
    echo Build failed.
    exit /b 1
)
