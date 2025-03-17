group "plugins/template"
	project "nvigi.plugin.template.inference.cuda"
		kind "SharedLib"	
		
		pluginBasicSetup("template.inference.cuda")
	
		files { 
			"./**.h",
			"./**.cpp",			
		}

		vpaths { ["impl"] = {"./**.h", "./**.cpp" }}
	
		includedirs {
			externaldir .."cuda//include", 
			"./"
		}

		libdirs {externaldir .."cuda//lib/x64"}

		filter {"system:windows"}
			links {"cuda.lib", "cudart.lib"}		
		filter {}

		-- if using 3rd party libs they go to under the "external" folder and must be added to the project.xml and uploaded to packman		

		-- here is how to setup additional libs for linking
		--libdirs {externaldir .."path to mylib"}
		--links {"mylib.lib"}
group ""
