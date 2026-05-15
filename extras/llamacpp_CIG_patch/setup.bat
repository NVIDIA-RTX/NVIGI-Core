@echo off

REM Clone llama.cpp if it doesn't exist
if not exist llama.cpp (
    echo Cloning llama.cpp from GitHub...
    git clone https://github.com/ggml-org/llama.cpp
    if errorlevel 1 (
        echo Error: Failed to clone llama.cpp repository
        exit /b 1
    )
    
    pushd llama.cpp
    
    REM Checkout the specific commit
    echo Checking out specific commit 44dbe8c5218829be1603a08dee8847f4d0bca323...
    git checkout 44dbe8c5218829be1603a08dee8847f4d0bca323
    if errorlevel 1 (
        echo Error: Failed to checkout commit
        popd
        exit /b 1
    )
    
    REM Apply the modifications patch
    echo Applying custom modifications...
    git apply --whitespace=fix --ignore-whitespace ..\llama.cpp_modifications.patch
    if errorlevel 1 (
        echo Error: Failed to apply patch
        echo.
        echo Troubleshooting:
        echo   - Ensure the patch file has Unix LF line endings
        echo   - Verify you are on the correct commit
        echo   - Run regenerate_patch.bat to recreate the patch file
        popd
        exit /b 1
    )
    
    popd
    echo llama.cpp setup complete.
) else (
    echo llama.cpp directory already exists, skipping clone and patch.
)

REM Copy CIG files required to build modified llama-bench
set nvigi_core_dir=..\..
set cig_dir=%nvigi_core_dir%\external\cig_scheduler_settings
copy %cig_dir%\include\cig_scheduler_settings.h
copy %cig_dir%\include\common_scheduler_settings.h
copy %cig_dir%\bin\x64\Release\cig_scheduler_settings.dll
copy %nvigi_core_dir%\source\utils\nvigi.cig_compatibility_checker\CIG_compatibility_checker.h

REM Build llama.cpp using system CMake
REM Note that -DGGML_CUDA_NO_VMM=ON must be used, as CIG doesn't support CUDA virtual memory
echo Building llama.cpp...
pushd llama.cpp
cmake -B build -DLLAMA_CURL=OFF -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=80 -DGGML_CUDA_NO_VMM=ON -DLLAMA_BUILD_TOOLS=ON -DGGML_CUDA_FORCE_CUBLAS=ON "-DCMAKE_CXX_FLAGS=/DWITHOUT_IGI=1"
echo Building llama-bench target...
cmake --build build --config RelWithDebInfo --target llama-bench -j
if errorlevel 1 (
    echo Error: Failed to build llama-bench
    popd
    exit /b 1
)
popd

echo.
echo Build complete! llama-bench.exe should be at:
echo   llama.cpp\build\bin\RelWithDebInfo\llama-bench.exe
echo.

