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

		-- Platform-specific library directories
		filter {"platforms:x64"}
			includedirs {
				externaldir .."cuda//include", 
				externaldir .."cig_scheduler_settings//include", 
				externaldir .."vulkanSDK//Include",
				"./"
			}
			libdirs {
				externaldir .."cuda//lib/x64",
				externaldir .."vulkanSDK//Lib"
			}
		filter {}
    filter {"system:windows"}
		links {"cuda.lib", "cudart.lib", "vulkan-1.lib"}		
	filter {}

	-- Platform-specific DLL copying
	filter {"system:windows", "platforms:x64"}
		postbuildcommands {
		  '{COPYFILE} ../../external/cuda/bin/cudart64_12.dll %[%{cfg.buildtarget.directory}]',
		  '{COPYFILE} ../../external/cuda/extras/CUPTI/lib64/cupti64_2025.1.0.dll %[%{cfg.buildtarget.directory}]',
		  '{COPYFILE} ../../external/cig_scheduler_settings/bin/x64/Release/cig_scheduler_settings.dll %[%{cfg.buildtarget.directory}]'
		}
	filter {}
group ""

end
