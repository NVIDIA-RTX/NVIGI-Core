require("vstudio")

if os.ishost("windows") then
	function os.winSdkVersion()
		local reg_arch = iif( os.is64bit(), "\\Wow6432Node\\", "\\" )
		local sdk_version = os.getWindowsRegistry( "HKLM:SOFTWARE" .. reg_arch .."Microsoft\\Microsoft SDKs\\Windows\\v10.0\\ProductVersion" )
		if sdk_version ~= nil then return sdk_version end
	end
end

function get_include_string(includes)
    cmdString =" ";
    for k, v in ipairs(includes) do
        cmdString = cmdString.." -I \""..tostring(v).."\"";
    end
    return cmdString
end

function get_define_string(defines)
    defString =" ";
    for k, v in ipairs(defines) do
        defString = defString.." -D"..tostring(v);
    end
    return defString
end

function commaficate(options)
	if os.ishost("linux") then
		local result = "";
		local sep = "";
		for option in string.gmatch(options, "-%w+") do
			result = result..sep..tostring(option);
			sep = ","
		end
		return result;
	else
		return options;
	end
end

function generateDependencies(projects)
    local dependencies = {}
    for i, project in ipairs(projects) do
        dependencies[project.name] = {}
        for j, depProject in ipairs(projects) do
            if depProject ~= project then
                table.insert(dependencies[project.name], depProject.name)
            end
        end
    end
    return dependencies
end
