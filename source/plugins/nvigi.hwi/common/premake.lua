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
			"./"
		}
group ""

end
