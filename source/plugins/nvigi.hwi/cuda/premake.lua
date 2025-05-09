if os.istarget("windows") then

group "plugins/hwi"
	project "nvigi.plugin.hwi.cuda"
		kind "SharedLib"
		
		pluginBasicSetup("hwi/cuda")
	
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
	  '{COPYFILE} ../../external/cuda/bin/cudart64_12.dll %[%{cfg.buildtarget.directory}]',
	  '{COPYFILE} ../../external/cuda/extras/CUPTI/lib64/cupti64_2025.1.0.dll %[%{cfg.buildtarget.directory}]',
	  '{COPYFILE} ../../external/cig_scheduler_settings/bin/Release_x64/cig_scheduler_settings.dll %[%{cfg.buildtarget.directory}]'
	}
group ""

end
