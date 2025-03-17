ROOT = path.getabsolute("./").."/"
CORESDK = ROOT
include("premake/premake.utils.lua")
workspace "nvigicoresdk"
include("premake/premake.shared_config.lua")
include("premake/premake.shared_plugin.lua")

include("source/core/premake.lua")

include("source/plugins/nvigi.hwi/cuda/premake.lua")
include("source/plugins/nvigi.hwi/common/premake.lua")

include("source/plugins/nvigi.template.generic/premake.lua")
include("source/plugins/nvigi.template.inference/premake.lua")
include("source/plugins/nvigi.template.inference.cuda/premake.lua")

include("source/tests/ai/premake.lua")

include("source/tools/utils/premake.lua")
