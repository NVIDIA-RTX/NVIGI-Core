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

	includedirs {ROOT .. "source/core/nvigi.api", ROOT .. "source/utils/nvigi.ai"}

	if os.istarget("windows") then
		includedirs {
			externaldir .."cuda//include"
		}

		libdirs {externaldir .."cuda//lib/x64"}

		filter {"system:windows"}
			links { "WS2_32.lib", "d3d12.lib", "dxgi.lib", "dxguid.lib", "cuda.lib"}
			linkoptions{"/STACK:67108864"}
		filter {}
	end

	filter {"system:windows"}
		vpaths { ["impl"] = {"./**.h","./**.cpp", }}
		links { "dsound.lib", "winmm.lib"}
	filter {"system:linux"}
		links {"dl", "pthread", "rt"}
	filter {}

group ""