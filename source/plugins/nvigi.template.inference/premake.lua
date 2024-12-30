group "plugins/template"
	project "nvigi.plugin.template.inference"
		kind "SharedLib"	
		
		pluginBasicSetup("template.inference")
	
		files { 
			"./**.h",
			"./**.cpp",			
		}

		vpaths { ["impl"] = {"./**.h", "./**.cpp" }}
	
		-- if using 3rd party libs they go to under the "external" folder and must be added to the project.xml and uploaded to packman		

		-- here is how to setup additional libs for linking
		--libdirs {externaldir .."path to mylib"}
		--links {"mylib.lib"}
group ""
