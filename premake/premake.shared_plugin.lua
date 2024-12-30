function pluginBasicSetupInternal(name, sdk)
	targetdir (bindir .. "%{cfg.buildcfg}_%{cfg.platform}")
	implibdir  (artifactsdir .. "%{prj.name}/%{cfg.buildcfg}_%{cfg.platform}")
	objdir (artifactsdir .. "%{prj.name}/%{cfg.buildcfg}_%{cfg.platform}") 
	filter {"system:windows"}
		symbolspath (symbolsdir .. "%{cfg.buildcfg}_%{cfg.platform}/$(TargetName).pdb")
	filter {}

	files { 
		CORESDK .. "source/core/nvigi.api/**.h",
		CORESDK .. "source/core/nvigi.log/**.h",
		CORESDK .. "source/core/nvigi.types/**.h",
		CORESDK .. "source/core/nvigi.file/**.h",
		CORESDK .. "source/core/nvigi.extra/**.h",
		CORESDK .. "source/core/nvigi.plugin/**.h",
		CORESDK .. "source/core/nvigi.plugin/**.cpp",
		CORESDK .. "source/utils/**.h",
		ROOT .. "source/plugins/nvigi."..name.."/versions.h",
		ROOT .. "source/plugins/nvigi."..name.."/resource.h",
	}
		
	includedirs 
	{ 
		CORESDK .. "source/core",
		CORESDK .. "source/utils",
		CORESDK .. "source/core/nvigi.api",
		CORESDK .. "source/utils/nvigi.ai"
	}

	if sdk == False then
		dependson { "nvigi.core.framework"}
	end
	
	filter {"system:windows"}
		files {	ROOT .. "source/plugins/nvigi."..name.."/**.rc"}
		vpaths { ["api"] = {CORESDK .. "source/core/nvigi.api/**.h"}}
		vpaths { ["types"] = {CORESDK .. "source/core/nvigi.types/**.h"}}
		vpaths { ["log"] = {CORESDK .. "source/core/nvigi.log/**.h",CORESDK .. "source/core/nvigi.log/**.cpp"}}
		vpaths { ["utils/ai"] = {CORESDK .. "source/utils/nvigi.ai/*.h"}}
		vpaths { ["utils/dsound"] = {CORESDK .. "source/utils/nvigi.dsound/*.h"}}
		vpaths { ["file"] = {CORESDK .. "source/core/nvigi.file/**.h", CORESDK .. "source/core/nvigi.file/**.cpp"}}
		vpaths { ["extra"] = {CORESDK .. "source/core/nvigi.extra/**.h"}}
		vpaths { ["plugin"] = {CORESDK .. "source/core/nvigi.plugin/**.h",CORESDK .. "source/core/nvigi.plugin/**.cpp"}}
		vpaths { ["version"] = {ROOT .. "source/plugins/nvigi."..name.."/resource.h",ROOT .. "source/plugins/nvigi."..name.."/versions.h",ROOT .. "source/plugins/nvigi."..name.."/**.rc"}}
		
		-- NOTE: moved the warning section after the files setup because filter was messing up inclusion of *.rc
		
		-- disable warnings coming from external source code
		externalincludedirs { ROOT .. "source/plugins/nvigi."..name.."/external" }
		-- Apply settings to suppress warnings for external files
		filter { "files:external/**.cpp or files:**/external/**.cpp or files:external/**.c or files:**/external/**.c or files:**/external/**.cc or files:external/**.cc or files:**/external/**.cu or files:external/**.cu" }
			warnings "Off"
	filter {}
end

function pluginBasicSetupSDK(name)
	pluginBasicSetupInternal(name, True)
end

function pluginBasicSetup(name)
	pluginBasicSetupInternal(name, False)
end
