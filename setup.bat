@echo off
set cfg=vs2022
set platforms=--x64
set comps=""
:loop
IF NOT "%1"=="" (
    IF "%1"=="vs2019" (
        SET cfg=vs2019
    ) ELSE IF "%1"=="vs2022" (
        SET cfg=vs2022
    ) ELSE IF "%1"=="-components" (
        SET comps=-components %2
        SHIFT
    ) ELSE IF "%1"=="-x64" (
        REM For now, overwrite any other platfom - one platform at a time for now
        SET platforms=--x64
    ) ELSE (
        REM Unrecognized switch, print error message and exit
        echo Error: Unrecognized switch: %1
        exit /b 1
    )
    SHIFT
    GOTO :loop
)

ECHO Setting up for Platform %platforms%

IF "%cfg%"=="" (
    IF exist .\_project\vs2022\aiinferencemananger.sln (
        SET cfg=vs2022
    ) ELSE IF exist .\_project\vs2019\aiinferencemananger.sln (
        SET cfg=vs2019
    )
)

REM Pull the basic tools we need for the next steps
call .\tools\packman\packman.cmd pull -p windows-x86_64 .\tools\project.tools.xml

if exist .\scripts\setup_extra.bat call .\scripts\setup_extra.bat %platforms% %comps:"=%
if exist project.xml call .\tools\packman\packman.cmd pull -p windows-x86_64 project.xml

echo Creating project files for %cfg%
call .\tools\premake5\premake5.exe %cfg% %platforms% --file=.\premake.lua
