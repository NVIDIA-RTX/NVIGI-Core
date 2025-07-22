if os.istarget("windows") then

group "plugins/hwi"
	project "nvigi.plugin.hwi.d3d12"
		kind "SharedLib"
		
		pluginBasicSetup("hwi/d3d12")
	
		files {
			"./*.h",
			"./*.cpp"
		}

		vpaths { ["impl"] = {"./*.h", "./*.cpp" }}

		includedirs {
			externaldir .."cig_scheduler_settings//include", 
			externaldir .."dx_compute_scheduling_dummy_nvapi", 
			"./"
		}

		libdirs {
		}

		links { ROOT .. "external/nvapi/amd64/nvapi64.lib"}

	filter {}

	postbuildcommands {
	  '{COPYFILE} ../../external/cig_scheduler_settings/bin/Release_x64/cig_scheduler_settings.dll %[%{cfg.buildtarget.directory}]'
	}
group ""

end
