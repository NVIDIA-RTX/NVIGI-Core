group "core"

project "nvigi.core.framework"
    kind "SharedLib"
    targetdir (bindir .. "%{cfg.buildcfg}_%{cfg.platform}")
    implibdir  (libdir .. "%{cfg.buildcfg}_%{cfg.platform}")
    objdir (artifactsdir .. "%{prj.name}/%{cfg.buildcfg}_%{cfg.platform}")
    filter {"system:windows"}
            symbolspath (symbolsdir .. "%{cfg.buildcfg}_%{cfg.platform}/$(TargetName).pdb")
    filter {}
	
	staticruntime "off"
	
	if os.ishost("windows") then
		prebuildcommands { 'pushd '..path.translate(ROOT .. "_artifacts"), path.translate("../tools/").."gitVersion.bat", 'popd' }
	else
		prebuildcommands { '(cd '..path.translate(ROOT .. "_artifacts &&")..path.translate("../tools/").."gitVersion.sh)"}
	end
	
	files { 		
		"./nvigi.framework/**.h", 
		"./nvigi.framework/**.cpp", 				
		"./nvigi.api/**.h",
		"./nvigi.log/**.h",
		"./nvigi.log/**.cpp",
		"./nvigi.memory/**.h",
		"./nvigi.memory/**.cpp",
		"./nvigi.system/**.h",
		"./nvigi.system/**.cpp",
		"./nvigi.exception/**.h",
		"./nvigi.exception/**.cpp",		
		"./nvigi.plugin/**.h",
	}

	filter {"system:windows"}
		files {
			"./nvigi.framework/**.rc"
		}
	filter {}

	filter {"system:linux"}
		links {"dl", "pthread", "rt"}
	filter {"system:windows", "configurations:not Production"}
		defines { "NVIGI_VALIDATE_MEMORY" }
	filter {"system:windows"}
		links {ROOT .. "external/nvapi/amd64/nvapi64.lib", "dxgi.lib", "Version.lib", "dbghelp.lib"}
		linkoptions { "/DEF:" .. "\"" .. ROOT .. "source/core/nvigi.framework/exports.def\"" }
	
		vpaths { ["api"] = {"./nvigi.api/**.h"}}
		vpaths { ["log"] = {"./nvigi.log/**.h","./nvigi.log/**.cpp"}}
		vpaths { ["memory"] = {"./nvigi.memory/**.h","./nvigi.memory/**.cpp"}}
		vpaths { ["system"] = {"./nvigi.system/**.h","./nvigi.system/**.cpp"}}
		vpaths { ["framework"] = {"./nvigi.framework/**.cpp", "./nvigi.framework/framework.h"}}
		vpaths { ["exception"] = {"./nvigi.exception/**.h","./nvigi.exception/**.cpp"}}			
		vpaths { ["plugin"] = {"./nvigi.plugin/**.h","./nvigi.plugin/**.cpp"}}			
		vpaths { ["version"] = {"./nvigi.framework/versions.h","./nvigi.framework/resource.h","./nvigi.framework/**.rc"}}
	filter {}
		
	if os.ishost("windows") then
		postbuildcommands {
			'{COPYFILE} ../../external/amd-ags/ags_lib/lib/amd_ags_x64.dll %[%{cfg.buildtarget.directory}]'
		}
	end

group ""
