group "tests"

project "nvigi.test"
	kind "ConsoleApp"
	targetdir (bindir .. "%{cfg.platform}/%{cfg.buildcfg}")
	objdir (artifactsdir .. "%{prj.name}/%{cfg.platform}/%{cfg.buildcfg}")
	filter {"system:windows"}
		symbolspath (symbolsdir .. "%{cfg.platform}/%{cfg.buildcfg}/$(TargetName).pdb")
	filter {"system:windows", "action:vs*"}
		buildoptions { "/bigobj" }
	filter {}
	

	dependson { "nvigi.core.framework"}

	--floatingpoint "Fast"	

	files {
		"./**.h",
		"./**.cpp",
	}

	includedirs {
		ROOT .. "source/core/nvigi.api", 
		ROOT .. "source/utils/nvigi.ai", 
		ROOT .. "source/plugins/nvigi.hwi/cuda"
	}

	-- Platform-specific library directories
	filter {}
	filter {"system:windows", "platforms:x64"}
		includedirs {
			externaldir .."cuda/include", 
			externaldir .."cuda/extras/CUPTI/include", 
			externaldir .."cig_scheduler_settings/include",
			externaldir .."vulkanSDK//Include"
		}
		libdirs {
			externaldir .."cuda//lib/x64", 
			externaldir .."cuda/extras/CUPTI/lib64",
			externaldir .."vulkanSDK/Lib"
		}
	filter {}

	filter {"system:windows"}
		links { "WS2_32.lib", "d3d12.lib", "dxgi.lib", "dxguid.lib", "cuda.lib", "cupti.lib", "vulkan-1.lib", "delayimp.lib"}
		linkoptions{"/STACK:67108864", "/DELAYLOAD:nvcuda.dll"}
	filter {}

	filter {"system:windows"}
		vpaths { ["impl"] = {"./**.h","./**.cpp", }}
		links { "dsound.lib", "winmm.lib"}
	filter {}
group ""
