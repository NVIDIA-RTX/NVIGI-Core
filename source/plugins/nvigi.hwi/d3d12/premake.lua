-- SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
-- SPDX-License-Identifier: MIT

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
-- NV_ARM64_BEGIN
		filter {"system:windows", "platforms:arm64"}
			includedirs {
				externaldir .. "nvapi",
				externaldir .."cig_scheduler_settings_arm64//include", 
				"./"
			}
			links { ROOT .. "external/nvapi/aarch64/nvapia64.lib"}
-- NV_ARM64_END
		filter {}

	filter {"platforms:x64"}
		postbuildcommands {
		  '{COPYFILE} ../../external/cig_scheduler_settings/bin/Release_x64/cig_scheduler_settings.dll %[%{cfg.buildtarget.directory}]'
		}
-- NV_ARM64_BEGIN
	filter {"platforms:arm64"}
		postbuildcommands {
		  '{COPYFILE} ../../external/cig_scheduler_settings_arm64/bin/Release_arm64/cig_scheduler_settings.dll %[%{cfg.buildtarget.directory}]'
		}
-- NV_ARM64_END
	filter {}
group ""

end
