	include ("premake.utils.lua")

	-- _ACTION is the argument you passed into premake5 when you ran it.
	local project_action = "UNDEFINED"
	if _ACTION ~= nill then project_action = _ACTION end

	-- Where the project files (vs project, solution, etc) go
	location( ROOT .. "_project/" .. project_action)

	configurations { "Debug", "Production", "Release" }
	platforms { "x64" }
	architecture "x64"
	language "c++"
	preferredtoolarchitecture "x86_64"
	targetprefix ""
	flags {"FatalWarnings"}

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
			linkoptions {"/HIGHENTROPYVA", "/CETCOMPAT"}
	filter{}
	
	filter {"system:windows"}
		externalincludedirs { externaldir }
		externalwarnings "Off"
		defines { "NVIGI_SDK", "NVIGI_WINDOWS", "WIN32" , "WIN64" , "_CONSOLE", "NOMINMAX"}
		buildoptions {"/utf-8", "/Zc:__cplusplus" }
		defines {
			"_CRT_SECURE_NO_WARNINGS"
		}		
    filter {"system:linux"}
		defines { "NVIGI_SDK", "NVIGI_LINUX" }
		-- stop on first error
		buildoptions {"-std=c++2b", "-Wfatal-errors","-fPIC", "-Wall", "-Wextra" , "-Wpedantic", "-Wcast-qual", "-Wdouble-promotion",
				      "-Wshadow",  "-Wpointer-arith", "-pthread", "-march=native", "-mtune=native", "-fvisibility=hidden","-finput-charset=UTF-8", "-fexec-charset=UTF-8"}					  
        linkoptions { "-Wl,--no-undefined" }
    filter { "files:**.cpp", "system:linux"}
    	buildoptions {"-fpermissive","-Wstrict-prototypes","-Wmissing-prototypes"}
	-- when building any visual studio project
	filter {"system:windows", "action:vs*"}
		flags { "MultiProcessorCompile", "NoMinimalRebuild"}		
    filter {}

    filter {"system:windows"}
		cppdialect "C++latest"
	filter {"system:linux"}
		cppdialect "C++2a"
	filter {} 
	
	filter "configurations:not Production"
		defines { "NVIGI_VALIDATE_MEMORY" }
	filter "configurations:Debug"
		defines { "DEBUG", "_DEBUG", "NVIGI_ENABLE_TIMING=1", "NVIGI_DEBUG", "NVIGI_VALIDATE_MEMORY" }
		symbols "Full"
	filter {"configurations:Debug","system:linux"} 
		buildoptions {"-g", "-O0"}
	filter {} 
				
	filter "configurations:Release"
		defines { "NDEBUG", "NVIGI_ENABLE_TIMING=1", "NVIGI_RELEASE" }
		optimize "On"
		symbols "On"

	filter "configurations:Production"
		defines { "NDEBUG","NVIGI_ENABLE_TIMING=0","NVIGI_ENABLE_PROFILING=0","NVIGI_PRODUCTION" }
		optimize "On"
		symbols "On"
		flags { "LinkTimeOptimization" }
		
	filter {} -- clear filter when you know you no longer need it!
	    filter {"system:windows"}
		defines { 
			"NVIGI_DEF_MIN_OS_MAJOR=10",
			"NVIGI_DEF_MIN_OS_MINOR=0",
			"NVIGI_DEF_MIN_OS_BUILD=19041"
		}
    filter {"system:linux"}
		defines { 
			"NVIGI_DEF_MIN_OS_MAJOR=20",
			"NVIGI_DEF_MIN_OS_MINOR=04",
			"NVIGI_DEF_MIN_OS_BUILD=0"
		}
	filter {}
    filter {"system:windows"}
		defines { 
			"NVIGI_CUDA_MIN_GPU_ARCH=0x00000140",
			"NVIGI_CUDA_MIN_DRIVER_MAJOR=551",
			"NVIGI_CUDA_MIN_DRIVER_MINOR=78",
			"NVIGI_CUDA_MIN_DRIVER_BUILD=0"
		}
    filter {"system:linux"}
		defines { 
			"NVIGI_CUDA_MIN_GPU_ARCH=0x00000140",
			"NVIGI_CUDA_MIN_DRIVER_MAJOR=531",
			"NVIGI_CUDA_MIN_DRIVER_MINOR=14",
			"NVIGI_CUDA_MIN_DRIVER_BUILD=0"
		}
	filter {}
