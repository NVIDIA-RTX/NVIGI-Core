-- SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
-- SPDX-License-Identifier: MIT
if os.istarget("windows") then
    group "plugins/hwi"
    project "nvigi.plugin.hwi.vulkan"
        kind "SharedLib"
        pluginBasicSetup("hwi/vulkan")
        files { "./*.h", "./*.cpp" }
        vpaths { ["impl"] = { "./*.h", "./*.cpp" } }
        includedirs { "./" }
        filter { "platforms:x64" }
            includedirs { externaldir .. "vulkanSDK/include" }
        filter { "platforms:arm64" }
            includedirs { externaldir .. "vulkanSDK_arm64/Include" }
        filter {}
    group ""
end
