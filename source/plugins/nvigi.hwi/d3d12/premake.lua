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

		filter {"system:windows", "platforms:x64"}
			includedirs {
				externaldir .. "nvapi",
				externaldir .."cig_scheduler_settings//include", 
				"./"
			}
			links { ROOT .. "external/nvapi/amd64/nvapi64.lib"}
		filter {}

	filter {"platforms:x64"}
		postbuildcommands {
		  '{COPYFILE} ../../external/cig_scheduler_settings/bin/x64/Release/cig_scheduler_settings.dll %[%{cfg.buildtarget.directory}]'
		}
	filter {}
group ""

end
