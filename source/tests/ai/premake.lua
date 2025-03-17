group "tests"

project "nvigi.test"
	kind "ConsoleApp"
	targetdir (bindir .. "%{cfg.buildcfg}_%{cfg.platform}")
	objdir (artifactsdir .. "%{prj.name}/%{cfg.buildcfg}_%{cfg.platform}") 
	filter {"system:windows"}
		symbolspath (symbolsdir .. "%{cfg.buildcfg}_%{cfg.platform}/$(TargetName).pdb")
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
	includedirs {
		externaldir .."cuda/include", 
		externaldir .."cuda/extras/CUPTI/include", 
		externaldir .."cig_scheduler_settings/include"}

	if os.istarget("windows") then
		libdirs {
			externaldir .."cuda//lib/x64", 
			externaldir .."cuda/extras/CUPTI/lib64",
			externaldir .."cig_scheduler_settings/lib/Release_x64"}

		filter {"system:windows"}
			links { "WS2_32.lib", "d3d12.lib", "dxgi.lib", "dxguid.lib", "cuda.lib", "cupti.lib"}
			linkoptions{"/STACK:67108864"}
		filter {}
	end

	filter {"system:linux"}
		disablewarnings "shadow"
	filter {}

	filter {"system:windows"}
		vpaths { ["impl"] = {"./**.h","./**.cpp", }}
		links { "dsound.lib", "winmm.lib"}
	filter {"system:linux"}
		libdirs {externaldir .."cuda/lib64/stubs", externaldir .."cuda/extras/CUPTI/lib64"}
		links {"dl", "pthread", "rt", "cuda"}
	filter {}

group ""
