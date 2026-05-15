	include ("premake.utils.lua")

	-- _ACTION is the argument you passed into premake5 when you ran it.
	local project_action = "UNDEFINED"
	if _ACTION ~= nill then project_action = _ACTION end

	-- Where the project files (vs project, solution, etc) go
	location( ROOT .. "_project/" .. project_action)

	newoption {
		trigger = "x64",
		description = "Enable building of x64"
	}

	if _OPTIONS["x64"] ~= nil then
		print("Enabling x64 build support")
		platforms { "x64" }
	end


	configurations { "Debug", "Production", "Release" }


	-- Platform-specific architecture settings
	filter { "platforms:x64" }
		architecture "x64"
	filter {}
	filter {}

	language "c++"
	preferredtoolarchitecture "x86_64"
	targetprefix ""
	fatalwarnings "All"

	externaldir = (ROOT .."external/")
	artifactsdir = (ROOT .."_artifacts/")
	bindir = (ROOT .."bin/")
	libdir = (ROOT .."lib/")
	symbolsdir = (ROOT .."symbols/")

	includedirs 
	{ 
		ROOT, CORESDK .. "source/core", CORESDK .. "include", ROOT.."external/json/source"
	}
   	 
	if os.ishost("windows") then
		local winSDK = os.winSdkVersion() .. ".0"
		print("WinSDK", winSDK)
		systemversion(winSDK)
	end

	-- DO NOT REMOVE, required for security --
	filter {"system:windows","configurations:Production"}
		buildoptions {"/guard:ehcont","/guard:cf","/sdl"}
		linkoptions {"/HIGHENTROPYVA"}
	filter {"system:windows","configurations:Production", "platforms:x64"}
		linkoptions {"/CETCOMPAT"}
	filter{}
	
	filter {"system:windows"}
		externalincludedirs { externaldir }
		externalwarnings "Off"
		defines { "NVIGI_SDK", "NVIGI_WINDOWS", "WIN32", "_CONSOLE", "NOMINMAX"}
		buildoptions {"/utf-8", "/Zc:__cplusplus" }

	-- Platform-specific Windows defines
	filter {"system:windows", "platforms:x64"}
		defines { "WIN64" }
	filter {"system:windows"}
		defines { "_CRT_SECURE_NO_WARNINGS" }
	-- when building any visual studio project
	filter {"system:windows", "action:vs*"}
		multiprocessorcompile "On"
		minimalrebuild "Off"
	filter {}

    filter {"system:windows"}
		cppdialect "C++latest"
	filter {} 
	
	filter "configurations:not Production"
		defines { "NVIGI_VALIDATE_MEMORY" }
	filter "configurations:Debug"
		defines { "DEBUG", "_DEBUG", "NVIGI_ENABLE_TIMING=1", "NVIGI_DEBUG", "NVIGI_VALIDATE_MEMORY" }
		symbols "Full"
	filter "configurations:Release"
		defines { "NDEBUG", "NVIGI_ENABLE_TIMING=1", "NVIGI_RELEASE" }
		optimize "On"
		symbols "On"
	filter "configurations:Production"
		defines { "NDEBUG","NVIGI_ENABLE_TIMING=0","NVIGI_ENABLE_PROFILING=0","NVIGI_PRODUCTION" }
		optimize "On"
		symbols "On"
		linktimeoptimization "On"
		
	filter {} -- clear filter when you know you no longer need it!
	filter {"system:windows"}
		defines { 
			"NVIGI_DEF_MIN_OS_MAJOR=10",
			"NVIGI_DEF_MIN_OS_MINOR=0",
			"NVIGI_DEF_MIN_OS_BUILD=19041",
			"NVIGI_CUDA_MIN_GPU_ARCH=0x00000140",
			"NVIGI_CUDA_MIN_DRIVER_MAJOR=551",
			"NVIGI_CUDA_MIN_DRIVER_MINOR=78",
			"NVIGI_CUDA_MIN_DRIVER_BUILD=0"
		}
	filter {}
