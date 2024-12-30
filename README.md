# NVIDIA In-Game Inference Core

Version 1.0.0

In-Game Inferencing (NVIGI) is an open-sourced cross platform solution that simplifies integration of the latest NVIDIA and other provider's technologies into applications and games. This framework allows developers to easily implement one single integration and enable multiple technologies supported by the hardware vendor or cloud provider. Supported technologies include AI inference but, due to the generic nature of the NVIGI SDK, can be expanded to graphics or any other field of interest.

For high-level details of NVIGI, as well as links to the official downloads for the priduct, see [NVIDIA In-Game Inferencing](https://developer.nvidia.com/rtx/in-game-inferencing)

This directory contains the core API used by applications and plugins; all NVIGI-based applications and plugins are built against this component.

> **IMPORTANT:**
> For important changes and bug fixes included in the current release, please see the release notes at the end of this document BEFORE use.

## Prerequisites

### Hardware

- NVIDIA RTX 30x0/A6000 series or (preferably) RTX 4080/4090 or RTX 5080/5090 with a minimum of 8GB and recommendation of 12GB VRAM.  

> NOTE: Some plugins only support RTX 40x0 and above, and will not be available on RTX 30x0.

### Windows

- Win10 20H1 (version 2004 - 10.0.19041) or newer
- Install graphics driver r555.85 or newer
- Install VS2022  or VS Code with [SDK 10.0.19041+](https://go.microsoft.com/fwlink/?linkid=2120843)

## Basic Steps: Summary

The following sections detail each of the steps of how to set up, rebuild and run the samples in this pack from the debugger or command line.  But the basic steps are:
1. Generate the project files
   1. Open a VS2022 Development Command Prompt to the repo directory (same directory as this file)
   2. Run `setup.bat`
> NOTE:
> If `setup.bat` fails with an error from the `packman` package downloader, please re-run `setup.bat` again as there are rare but possible issues with link-creation on initial run.


2. Build the project
   1. Launch VS2022
   2. Load `sdk/_project/vs2022/nvigicoresdk.sln` as the workspace
   3. Build ALL configurations (Debug, Release, Production) manually or using Batch Build...
3. Run the test app
   1. Open a (or use an existing) VS2022 Development Command Prompt to therepo directory (same directory as this file)
      2. Run `bin\Release_x64\nvigi.test.exe`
4. Package the core "SDK" if needed for use case

## Setup 

To setup the NVIGI Core repository for building all components and running the test, open a command prompt window or PowerShell to the same directory as this file and simply run the following command:

```sh
setup.bat
```

> **IMPORTANT:** 
> These steps are listed in the following section are done for you by the `setup.bat` script - **do not run these steps manually/individually**.  This is merely to explain what is happening for you.

When you run the `setup.bat` script, the script will cause two things to be done for you:

1. The NVIDIA tool `packman` will pull all build dependencies to the local machine and cache them in a shared directory.  Links will be created from `external` in the NVIGI tree to this shared cache for external build dependencies.
2. `premake` will generate the project build files in `_project\vs20XX` (for Windows)

> NOTE:
> If `setup.bat` fails with an error from the `packman` package downloader, please re-run `setup.bat` again as there are rare but possible issues with link-creation on initial run.

## Building

To build the project, simply open `_project\vs20XX\nvigicoresdk.sln` in Visual Studio, select the desired build configuration and build.  In general, when building core it is best to build all three configurations (Core builds quickly)

> NOTE: To build the project minimal configuration is needed. Any version of Windows 10 will do. Then
run the setup and build scripts as described here above. That's it. The specific version of Windows, and NVIDIA driver 
are all runtime dependencies, not compile/link time dependencies. This allows NVIGI to build on stock
virtual machines that require zero configuration.

## Tests

### Configuring and Running the Test

The built-in `nvigi.test.exe` is the Core unit test suite.  It uses the open source Catch2 framework to run basic tests and log the status.

#### Run at Command Line

To run `nvigi.test.exe` from the command line, take the following steps:

1. Open a command prompt in the same directory as this readme
2. Run the command:
```sh
bin\Release_x64\nvigi.test.exe
```

#### Run in Debugger

1. Open the project solution and build the desired configuration
2. Set the default project to tests/nvigi.test
3. Launch the project in the debugger
4. Note the log output

## Packaging

The Core SDK component is packaged into two distinct types of package: Runtime SDK and Plugin Development Kit (PDK).  The former is designed to be used by NVIGI applications; the latter is to be used for developing/building plugins.

The PDK is basically a subset of what is created by simply building the NVIGI Core source repo.  Headers are expected in their source locations, the binaries are built into their expected locations.  So, having built all configurations of NVIGI Core, the root of core **already** contains everything a PDK needs (i.e. `bin`, `lib`, `source` etc), and more.  I.e. once you have built all configs, `<CORE_ROOT>` can be used as the root of a PDK.

However, if desired, a more minimal, consolidated Core PDK **can** be packaged into any desired directory using the `package.bat` script.  Such a packaged PDK is easier to zip and reuse, if desired, and contains only the subset of a core source tree that is **needed** as a PDK.  This can be done by running the `package.bat` script after building all configurations of Core:
```sh
package.bat -config pdk -dir _pdk
```

Which will package the Runtime SDK into `_pdk` (and could be used by other trees making a link to the `_pdk` directory as `nvigi_core`)

The Runtime SDK **must** be packaged via the packaging script.  This can be done by running the `package.bat` script after building all configurations of Core:
```sh
package.bat -config runtime -dir _sdk
```

Which will package the Runtime SDK into `_sdk`

## Changing an Existing Project

> IMPORTANT: Do not edit the MSVC project files directly!  Always modify the `premake.lua` or files in `premake`.

When changing an existing project's settings or contents (ie: adding a new source file, changing a compiler setting, linking to a new library, etc), it is necessary to run `setup.bat` again for those changes to take effect and MSVC project files and or solution will need to be reloaded in the IDE.

> NOTE: NVIDIA does not recommend making changes to the headers in core, as these can affect the API itself and can make developer-built components incompatible with NVIDIA-supplied components.

## Release Notes:
- TBD 
