@echo off
setlocal

if exist .\_project\vs2022\nvigicoresdk.sln (
    rem find VS 2022
    for /f "usebackq tokens=1* delims=: " %%i in (`.\tools\vswhere.exe -version [17^,18^) -requires Microsoft.VisualStudio.Workload.NativeDesktop`) do (
        if /i "%%i"=="installationPath" (
            set VS_PATH=%%j
            GOTO :found_vs
        )
    )
    rem Try looking for VS Build Tools 2022
    for /f "usebackq tokens=1* delims=: " %%i in (`.\tools\vswhere.exe -products * -version [17^,18^) -requires Microsoft.VisualStudio.Workload.VCTools -requires Microsoft.VisualStudio.Workload.MSBuildTools`) do (
        if /i "%%i"=="installationPath" (
            set VS_PATH=%%j
            GOTO :found_vs
        )
    )
) else if exist .\_project\vs2019\nvigicoresdk.sln (
    rem find VS 2019
    for /f "usebackq tokens=1* delims=: " %%i in (`.\tools\vswhere.exe -version [16^,17^) -requires Microsoft.VisualStudio.Workload.NativeDesktop `) do (
        if /i "%%i"=="installationPath" (
            set VS_PATH=%%j
            GOTO :found_vs
        )
    )
    rem Try looking for VS Build Tools 2019
    for /f "usebackq tokens=1* delims=: " %%i in (`.\tools\vswhere.exe -products * -version [16^,17^) -requires Microsoft.VisualStudio.Workload.VCTools -requires Microsoft.VisualStudio.Workload.MSBuildTools`) do (
        if /i "%%i"=="installationPath" (
            set VS_PATH=%%j
            GOTO :found_vs
        )
    )
) else (
    rem find VS 2017
    for /f "usebackq tokens=1* delims=: " %%i in (`.\tools\vswhere.exe -version [15^,16^) -requires Microsoft.VisualStudio.Workload.NativeDesktop`) do (
        if /i "%%i"=="installationPath" (
            set VS_PATH=%%j
            GOTO :found_vs
        )
    )
)
:found_vs

echo off
set cfgs=Debug
set bld=Clean,Build

:loop
IF NOT "%1"=="" (
    IF "%1"=="-fast" (
        set bld=Build
    )
    IF "%1"=="-debug" (
        SET cfgs=Debug
    )
    IF "%1"=="-release" (
        SET cfgs=Release
    )
    IF "%1"=="-production" (
        SET cfgs=Production
    )    
    IF "%1"=="-all" (
        SET cfgs=Debug Release Production
    )    
    SHIFT
    GOTO :loop
)

if not exist "%VS_PATH%" (
    echo "%VS_PATH%" not found. Is Visual Studio installed? && goto :ErrorExit
)

for /f "delims=" %%F in ('dir /b /s "%VS_PATH%\vsdevcmd.bat" 2^>nul') do set VSDEVCMD_PATH=%%F
echo ********Executing %VSDEVCMD_PATH%********
call "%VSDEVCMD_PATH%"
goto :SetVSEnvFinished

:ErrorExit
exit /b 1

:SetVSEnvFinished

setlocal enabledelayedexpansion
for %%c in (%cfgs%) do (
    echo %%c
    if exist .\_project\vs2022\nvigicoresdk.sln (
        msbuild .\_project\vs2022\nvigicoresdk.sln /m /t:%bld% /property:Configuration=%%c
    ) else if exist .\_project\vs2019\nvigicoresdk.sln (
        msbuild .\_project\vs2019\nvigicoresdk.sln /m /t:%bld% /property:Configuration=%%c
    ) else (
        msbuild .\_project\vs2017\nvigicoresdk.sln /m /t:%bld% /property:Configuration=%%c
    )
)
