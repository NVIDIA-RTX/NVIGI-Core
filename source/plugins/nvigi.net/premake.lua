group "plugins/networking"

	project "nvigi.plugin.net"
		filter "platforms:x64"
			kind "SharedLib"
		filter {}
		
		pluginBasicSetup("net")

		files {
			"./*.h",
			"./*.cpp",
		}

		-- Hack to avoid warnings as errors for MSVCRT mismatch
       filter "toolset:msc*"
           linkoptions { "/WX:NO" }
       filter {}	

		vpaths { ["impl"] = {"./*.h","./*.cpp"}}
			
		libdirs {externaldir .."libcurl/lib/rt_dynamic/release"}

		filter {"system:windows", "configurations:Debug"}
			libdirs { externaldir .."/zlib/debug/lib"}
			links {"zlibd.lib"}
		filter {"system:windows", "configurations:not Debug"}
			libdirs { externaldir .."/zlib/lib"}
			links {"zlib.lib"}
		filter "system:windows"
			links {"libcurl.lib", "ws2_32.lib", "wldap32.lib","advapi32.lib","kernel32.lib","comdlg32.lib","crypt32.lib","normaliz.lib"}
		filter{}
group ""