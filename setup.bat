@echo off
set cfg=vs2022
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
    ) ELSE (
        REM Unrecognized switch, print error message and exit
        echo Error: Unrecognized switch: %1
        exit /b 1
    )
    SHIFT
    GOTO :loop
)

IF "%cfg%"=="" (
    IF exist .\_project\vs2022\aiinferencemananger.sln (
        SET cfg=vs2022
    ) ELSE IF exist .\_project\vs2019\aiinferencemananger.sln (
        SET cfg=vs2019
    )
)

REM Pull the basic tools we need for the next steps
call .\tools\packman\packman.cmd pull -p windows-x86_64 .\tools\project.tools.xml

if exist project.xml call .\tools\packman\packman.cmd pull -p windows-x86_64 project.xml

echo Creating project files for %cfg%
call .\tools\premake5\premake5.exe %cfg% --file=.\premake.lua
