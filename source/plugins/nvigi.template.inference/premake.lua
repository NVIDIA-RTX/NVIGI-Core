-- Modern AI Plugin Template - Build Configuration
-- ================================================
-- This premake configuration demonstrates how to set up a plugin with
-- conditional support for different GPU backends: CUDA, D3D12, and Vulkan.
--
-- To enable a specific backend, define the corresponding preprocessor macro:
-- - PLUGIN_USES_CUDA: Enable CUDA support (NVIDIA GPUs)
-- - PLUGIN_USES_D3D12: Enable DirectX 12 support (Windows)
-- - PLUGIN_USES_VULKAN: Enable Vulkan support (Cross-platform)
--
-- You can enable multiple backends simultaneously if needed.

group "plugins/template"
	project "nvigi.plugin.template.inference"
		kind "SharedLib"	
		
		pluginBasicSetup("template.inference")
	
		files { 
			"./**.h",
			"./**.cpp",			
		}	
		
		vpaths { ["impl"] = {"./**.h", "./**.cpp" }}
	
		includedirs {
			ROOT .. "source/plugins/nvigi.template.inference/backend",
			ROOT .. "source/plugins/nvigi.template.inference",
		}

		-- ====================================================================
		-- Platform-Specific Configuration
		-- ====================================================================
		-- Uncomment the backend(s) you need for your plugin

		-- --------------------------------------------------------------------
		-- CUDA Backend (NVIDIA GPUs)
		-- --------------------------------------------------------------------
		-- Uncomment to enable CUDA support:
		
		-- defines { "PLUGIN_USES_CUDA" }
		-- 
		-- includedirs {
		-- 	externaldir .."cuda//include", 
		-- 	"./"
		-- }
		-- 
		-- libdirs { externaldir .."cuda//lib/x64" }
		-- 
		-- filter {"system:windows"}
		-- 	links { "cuda.lib", "cudart.lib" }
		-- filter {"system:linux"}
		-- 	links { "cuda", "cudart" }
		-- filter {}

		-- --------------------------------------------------------------------
		-- D3D12 Backend (Windows DirectX 12)
		-- --------------------------------------------------------------------
		-- Uncomment to enable D3D12 support:
		
		-- defines { "PLUGIN_USES_D3D12" }
		-- 
		-- filter {"system:windows"}
		-- 	links { "d3d12.lib", "dxgi.lib", "dxguid.lib" }
		-- filter {}

		-- --------------------------------------------------------------------
		-- Vulkan Backend (Cross-platform)
		-- --------------------------------------------------------------------
		-- Uncomment to enable Vulkan support:
		
		-- defines { "PLUGIN_USES_VULKAN" }
		-- 
		-- includedirs {
		-- 	externaldir .."vulkan//include"
		-- }
		-- 
		-- libdirs { externaldir .."vulkan//lib" }
		-- 
		-- filter {"system:windows"}
		-- 	links { "vulkan-1.lib" }
		-- filter {"system:linux"}
		-- 	links { "vulkan" }
		-- filter {}

		-- ====================================================================
		-- Third-Party Libraries
		-- ====================================================================
		-- If your plugin uses external libraries (e.g., model backends,
		-- math libraries), add them here.
		--
		-- Important: External libraries must be:
		-- 1. Placed in the "external" folder
		-- 2. Added to project.xml
		-- 3. Uploaded to packman (for distribution)
		--
		-- Example:
		-- includedirs { externaldir .."mylib/include" }
		-- libdirs { externaldir .."mylib/lib/x64" }
		-- links { "mylib.lib" }

group ""
