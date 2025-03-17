if os.istarget("windows") then

group "plugins/hwi"
	project "nvigi.plugin.hwi.common"
		kind "SharedLib"
		
		pluginBasicSetup("hwi/common")
	
		files {
			"./*.h",
			"./*.cpp"
		}

		vpaths { ["impl"] = {"./*.h", "./*.cpp" }}

		includedirs {
			externaldir .."cuda//include", 
			externaldir .."cig_scheduler_settings//include", 
			"./"
		}

		libdirs {
			externaldir .."cuda//lib/x64",
			externaldir .."cig_scheduler_settings//lib/Release_x64"
		}

    filter {"system:windows"}
		links {"cuda.lib", "cudart.lib"}		
	filter {}

	postbuildcommands {
	  '{COPYFILE} ../../external/cuda/bin/cudart64_12.dll %[%{cfg.buildtarget.directory}]'
	}
group ""

end
