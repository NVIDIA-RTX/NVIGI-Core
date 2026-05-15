if os.istarget("windows") then

group "tools"

project "nvigi.tool.utils"
	kind "ConsoleApp"
       targetdir (bindir .. "%{cfg.platform}/%{cfg.buildcfg}")
       objdir (artifactsdir .. "%{prj.name}/%{cfg.platform}/%{cfg.buildcfg}")
       filter {"system:windows"}
               symbolspath (symbolsdir .. "%{cfg.platform}/%{cfg.buildcfg}/$(TargetName).pdb")
       filter {}
	
	--floatingpoint "Fast"

	files {
		"./**.h",
		"./**.cpp",
		ROOT .. "source/core/nvigi.memory/**.cpp", -- needed to test types
	}
	
	vpaths { ["impl"] = {"./**.h","./**.cpp", }}
	links {"rpcrt4.lib"}
		

group ""

end
