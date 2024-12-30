if os.istarget("windows") then

group "tools"

project "nvigi.tool.utils"
	kind "ConsoleApp"
       targetdir (bindir .. "%{cfg.buildcfg}_%{cfg.platform}")
       objdir (artifactsdir .. "%{prj.name}/%{cfg.buildcfg}_%{cfg.platform}")
       filter {"system:windows"}
               symbolspath (symbolsdir .. "%{cfg.buildcfg}_%{cfg.platform}/$(TargetName).pdb")
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
