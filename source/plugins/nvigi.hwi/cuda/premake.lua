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
			"./"
		}

		libdirs {externaldir .."cuda//lib/x64"}

    filter {"system:windows"}
		links {"cuda.lib", "cudart.lib"}		
	filter {}

	postbuildcommands {
	  '{COPYFILE} ../../external/cuda/bin/cudart64_12.dll %[%{cfg.buildtarget.directory}]'
	}
group ""

end
