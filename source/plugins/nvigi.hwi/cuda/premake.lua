-- SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
-- SPDX-License-Identifier: MIT

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
-- NV_ARM64_BEGIN
		filter {"platforms:arm64"}
			includedirs {
				externaldir .."cuda//include", 
				externaldir .."cig_scheduler_settings_arm64//include", 
				externaldir .."vulkanSDK_arm64//Include",
				"./"
			}
			libdirs {
				-- Note: CUDA ARM64 support may be limited, check if these paths exist
				externaldir .."cuda//lib/arm64",
				externaldir .."vulkanSDK_arm64//Lib-ARM64"
			}
-- NV_ARM64_END
		filter {}
    filter {"system:windows"}
		links {"cuda.lib", "cudart.lib", "vulkan-1.lib"}		
	filter {}

	-- Platform-specific DLL copying
	filter {"system:windows", "platforms:x64"}
		postbuildcommands {
		  '{COPYFILE} ../../external/cuda/bin/x64/cudart64_13.dll %[%{cfg.buildtarget.directory}]',
		  '{COPYFILE} ../../external/cuda/extras/CUPTI/lib/x64/cupti64_2026.3.0.dll %[%{cfg.buildtarget.directory}]',
		  '{COPYFILE} ../../external/cig_scheduler_settings/bin/Release_x64/cig_scheduler_settings.dll %[%{cfg.buildtarget.directory}]'
		}
-- NV_ARM64_BEGIN
	filter {"system:windows", "platforms:arm64"}
		-- Note: Check if ARM64 CUDA DLLs are available
		postbuildcommands {
		  '{COPYFILE} ../../external/cuda/bin/arm64/cudart64_13.dll %[%{cfg.buildtarget.directory}]',
		  '{COPYFILE} ../../external/cuda/extras/CUPTI/lib/arm64/cupti64_2026.3.0.dll %[%{cfg.buildtarget.directory}]',
		  '{COPYFILE} ../../external/cig_scheduler_settings_arm64/bin/Release_arm64/cig_scheduler_settings.dll %[%{cfg.buildtarget.directory}]'
		}
-- NV_ARM64_END
	filter {}
group ""

end
