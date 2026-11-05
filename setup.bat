:: SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
:: SPDX-License-Identifier: MIT

@echo off
setlocal EnableDelayedExpansion
set cfg=vs2022
set platforms=
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
        IF "%platforms%"=="" (
            SET platforms=--x64
        ) else (
            SET platforms=%platforms% --x64
        )
REM NV_ARM64_BEGIN
    ) ELSE IF "%1"=="-arm64" (
        IF "%platforms%"=="" (
            SET platforms=--arm64
        ) else (
            SET platforms=%platforms% --arm64
        )
REM NV_ARM64_END
    ) ELSE (
        REM Unrecognized switch, print error message and exit
        echo Error: Unrecognized switch: %1
        exit /b 1
    )
    SHIFT
    GOTO :loop
)

IF "%platforms%"=="" (
    SET platforms=--x64
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
if errorlevel 1 (
    echo ERROR: Build-tool dependency download failed.
    exit /b 1
)

if exist .\scripts\setup_extra.bat (
    call .\scripts\setup_extra.bat %platforms% %comps:"=%
    if errorlevel 1 exit /b 1
)
if exist project.xml (
    call .\tools\packman\packman.cmd pull -p windows-x86_64 project.xml
    if errorlevel 1 (
        echo ERROR: Project dependency download failed.
        exit /b 1
    )
)

echo Creating project files for %cfg%
call .\tools\premake5\premake5.exe %cfg% %platforms% --file=.\premake.lua
if errorlevel 1 (
    echo ERROR: Project generation failed.
    exit /b 1
)

exit /b 0
