import argparse
import copy
from functools import cmp_to_key
import glob
import json
import os
from pathlib import Path
import shutil
import stat
import sys
import nvigi_utils as utils

nlohmann_json_ext = {"dep":"nlohmann_json", "path":"external/json",
                           "items":[{"name":"nlohmann_json", "version":"3.10.5.1-b440a32a-windows-x86_64","platforms":"windows-x86_64"},
                                    {"name":"nlohmann_json", "version":"3.10.5.1-b440a32a-windows-x86_64","platforms":"linux-x86_64"}
                                   ]
                          }

catch2_ext = {"dep":"catch2", "path":"external/catch2",
                           "items":[{"name":"catch2", "version":"2.13.9-7011809a","platforms":"windows-x86_64"},
                                    {"name":"catch2", "version":"2.13.9-7011809a","platforms":"linux-x86_64"}]
                          }

cuda_ext = {"dep":"cuda", "path":"external/cuda", 
                           "items":[{"name":"cuda", "version":"v12.8.0-windows-x64","platforms":"windows-x86_64"},
                                    {"name":"cuda-linux-x86_64", "version":"12.1.0","platforms":"linux-x86_64"}
                                    ]
                          }

cig_scheduler_settings_ext = {"dep":"cig_scheduler_settings", "path":"external/cig_scheduler_settings", 
                           "items":[{"name":"cig_scheduler_settings_win64", "version":"202507021723-ac549a08-main","platforms":"windows-x86_64"}
                                    #{"path":"../utils/cig_scheduler_settings"}
                                    ]
                          }

nvapi_ext = {"dep":"nvapi", "path":"external/nvapi", 
                           "items":[{"name":"nvapi", "version":"r575-public-windows-x86_64","platforms":"windows-x86_64"}
                                    ]
                          }

agility_sdk_ext = {"dep":"agility-sdk-code", "path":"external/agility-sdk",
                            "items":[{"name":"agility-sdk-code", "version":"1.615.1-x64-windows","platforms":"windows-x86_64"}]
                    }

amd_ags_ext = {"dep":"amd-ags", "path":"external/amd-ags",
                            "items":[{"name":"amd-ags", "version":"6.2.0-x64-windows","platforms":"windows-x86_64"}]
                    }

vulkan_ext = {"dep":"vulkansdk", "path":"external/vulkanSDK",
                           "items":[{"name":"vulkansdk", "version":"1.4.321.1-windows-x86_64","platforms":"windows-x86_64"},
                                    {"name":"vulkansdk", "version":"1.4.321.1-linux_x86_64","platforms":"linux-x86_64"}
                                   ]
}

cuda12dlls = {'win-x64':[
    'external/cuda/bin/cudart64_12.dll'
    ]
}

cuptiDlls =  {'win-x64':[
    'external/cuda/extras/CUPTI/lib64/cupti64_2025.1.0.dll',
    'external/cig_scheduler_settings/bin/Release_x64/cig_scheduler_settings.dll'],
}

amd_agsDll =  {'win-x64':[
    'external/amd-ags/ags_lib/lib/amd_ags_x64.dll'],
}

def DeleteDirTree(dir):
    # Remove the directory and all its contents
    if os.path.exists(dir):
        # we see some read-only files, so we need to fix those before we can delete
        for dirpath, dirnames, filenames in os.walk(dir):
            os.chmod( dirpath, stat.S_IWRITE )
            for filename in filenames:
                os.chmod(os.path.join(dirpath, filename), stat.S_IWRITE)
        shutil.rmtree(dir)

def ReadonlyTree(dest_root, readonly):
    readonly_files = []
    readonly_files.extend((dest_root/'include/').glob('**/*.*'))
    readonly_files.extend((dest_root/'source/').glob('**/*.*'))
    for dest_file in readonly_files:
        if not os.path.isdir(dest_file):
            os.chmod(dest_file, stat.S_IREAD if readonly else stat.S_IWRITE)



# Returns sources_list, a list of (source, component_name) tuples
def ExtractCopySourcesList(component_list, component_names, category):
    sources_list = []
    for name in component_names:
        component = component_list[name]
        if category in component:
            for item in component[category]:
                sources_list.append((item, name))
    return sources_list

def ExtractCopySourcesListPlatformWithDest(component_list, component_names, category, platform, root):
    sources_list = []
    for name in component_names:
        component = component_list[name]
        if category in component and platform in component[category]:
            items = component[category][platform]
            for item in items:
                # If the item is a tuple, it represents (src file path, destination bin subdir)
                if isinstance(item, tuple):
                    item_path = root/item[0]
                    if '*' in str(item_path):
                        src_path = item_path.parent
                        wildcard = item_path.name
                        # Add wildcard items, assuming wildcards are in the filename only
                        for src in src_path.glob(wildcard):
                            if src.is_file()==True:
                                sources_list.append(((src, item[1]), name))
                    else:
                        sources_list.append(((item_path, item[1]), name))
                else:
                    item_path = root/item
                    if '*' in str(item_path):
                        src_path = item_path.parent
                        wildcard = item_path.name
                        # Add wildcard items, assuming wildcards are in the filename only
                        for src in src_path.glob(wildcard):
                            if src.is_file()==True:
                                sources_list.append((src, name))
                    else:
                        sources_list.append((item_path, name))
    return sources_list

# Returns sources_list, a list of (source, component_name) tuples
def ExtractCopySourcesListPlatform(component_list, component_names, category, platform, root):
    sources_list = []
    for name in component_names:
        component = component_list[name]
        if category in component and platform in component[category]:
            items = component[category][platform]
            for item in items:
                item_path = root/item
                if '*' in str(item_path):
                    src_path = item_path.parent
                    wildcard = item_path.name
                    # Add wildcard items, assuming wildcards are in the filename only
                    for src in src_path.glob(wildcard):
                        if src.is_file()==True:
                            sources_list.append((src, name))
                else:
                    sources_list.append((item_path, name))
    return sources_list

def WritePackmanPreamble(f):
    print('<project toolsVersion="6.21">', file=f)

def WritePackmanDep(dep, f):
    print(f'\t<dependency name="{dep["dep"]}" linkPath="{dep["path"]}">', file=f)
    for item in dep["items"]:
        if "version" in item:
            print(f'\t\t<package name="{item["name"]}" version="{item["version"]}" platforms="{item["platforms"]}"/>', file=f)
        elif "path" in item:
            print(f'\t\t<source path="{item["path"]}" />', file=f)
    print('\t</dependency>', file=f)
    
def WritePackmanCoda(f):
    print('</project>', file=f)

def ExtPairCompare(a, b):
    cmp = 0
    # argh: (a>b)-(a<b) replacing the removed cmp()
    aLower = str(a["dep"]).lower()
    bLower = str(b["dep"]).lower()
    if aLower != bLower:
        cmp = (aLower > bLower) - (aLower < bLower)
    return cmp

def WritePackman(externals, filename):
    sorted_externals = sorted(externals, key=cmp_to_key(ExtPairCompare))
    with open(filename, 'w') as f:
        WritePackmanPreamble(f)
        for item in sorted_externals:
            WritePackmanDep(item, f)
        WritePackmanCoda(f)

def WritePremake(premakes, filename, is_plugin_build=True):
    sorted_premakes = sorted(premakes)
    with open(filename, 'w') as f:
        print('ROOT = path.getabsolute("./").."/"', file=f)
        print('CORESDK = ROOT', file=f)
        print('include("premake/premake.utils.lua")', file=f)
        print('workspace "nvigicoresdk"', file=f)
        print('include("premake/premake.shared_config.lua")', file=f)
        print('include("premake/premake.shared_plugin.lua")', file=f)

        for item in sorted_premakes:
            print(f'include("{item}")', file=f)

def PackmanOnly(component_names, components, filename):
    externals = []
    for name in component_names:
        item = components[name]
        if "externals" in item:
            for dep in item["externals"]:
                duplicate = False
                for existing_dep in externals:
                    if dep["dep"] == existing_dep["dep"]:
                        duplicate = True
                        if dep != existing_dep:
                            print(f'Conflicting packman dependencies: {dep["dep"]}')
                if duplicate == False:
                    externals.append(dep)
        # We ONLY add packman packages for internal-only packages to the local build files,
        # NOT the packaged project.xml.  This is important if the items are to be distributed
        # in the click-through licensed nvigi_pack, and NOT from public packman
        if "externals_private" in item:
            for dep in item["externals_private"]:
                duplicate = False
                for existing_dep in externals:
                    if dep["dep"] == existing_dep["dep"]:
                        duplicate = True
                        if dep != existing_dep:
                            print(f'Conflicting packman dependencies: {dep["dep"]}')
                if duplicate == False:
                    externals.append(dep)
    WritePackman(externals, filename)

def PremakeOnly(component_names, components, filename):
    premakes = []
    for name in component_names:
        item = components[name]
        if "premake" in item:
            prem = item["premake"]
            duplicate = False
            for existing_prem in premakes:
                if prem == existing_prem:
                    duplicate = True
            if duplicate == False:
                premakes.append(prem)
    WritePremake(premakes, filename, True)

def parse_args():
    default_dest='_core_sdk'

    parser = argparse.ArgumentParser(
        description='NVIGI Core SDK Packager',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument('-dir',
                        dest='dest', 
                        help=f'Dest directory {default_dest}',
                        default=default_dest)
    
    default_config = 'pdk'
    parser.add_argument('-config',
                        dest='config', 
                        choices=['runtime', 'pdk', 'src'],
                        help=f'Config {default_config}',
                        default=default_config)
    
    parser.add_argument('-debug', 
                        dest='pack_debug', action='store_true', 
                        help=f'Package the debug config (default all configs)')
    parser.add_argument('-production', 
                        dest='pack_production', action='store_true', 
                        help=f'Package the production config (default all configs)')
    parser.add_argument('-release', 
                        dest='pack_release', action='store_true', 
                        help=f'Package the release config (default all configs)')
    parser.add_argument('-no-deletion',
                        dest='delete_sdk', action='store_false',
                        help=f'Skip deleting SDK')

    default_platform = "win-x64"
    parser.add_argument('-platform',
                        dest='platform', 
                        choices=['win-x64'],
                        help=f'Platform {default_platform}',
                        default=default_platform)

    default_packman = ""
    parser.add_argument('-packman-only', 
                        dest='packman_only',
                        help=f'Write full packman XML and exit (to {default_packman})',
                        default=default_packman)
    default_premake = ""
    parser.add_argument('-premake-only', 
                        dest='premake_only',
                        help=f'Write full premake XML and exit (to {default_premake})',
                        default=default_premake)

    args, unknown = parser.parse_known_args()
    
    return args

if __name__ == '__main__':
    args = parse_args()
    dest_root = Path(f'{args.dest}')

    src=Path('.')
    
    if src.exists() and dest_root.exists() and os.path.samefile(dest_root, src):
        print(f'source ({src}) and destination ({dest_root}) of packaging cannot be the same directory!')
        sys.exit(-1)

    copy_list = []
    rename_list = []

    all_plat = ['win-x64']
    win_plat = ['win-x64']

    components = {
        'core.framework': {
            'platforms': all_plat,
            'sharedlib': ['nvigi.core.framework'],
            'libs': ['nvigi.core.framework'],
            'includes': ['source/**/nvigi*.h'],
            'sources': ['core', 'utils', 'shared'],
            'runtime_sources': ['utils/nvigi.cig_compatibility_checker', 'utils/nvigi.dsound', 'utils/nvigi.wav'],
            'premake': 'source/core/premake.lua',
            '3rdparty': amd_agsDll,
            'docs': ['Architecture.md', 'PluginDevelopment.md', 'GpuSchedulingForAI.md', 'HybridAI.md', 'ProgrammingGuide.md', 'ProgrammingGuideAI.md'],
            'externals': [agility_sdk_ext, amd_ags_ext, nlohmann_json_ext, nvapi_ext]
        },
        'plugin.hwi.common' : {
            'platforms': win_plat,
            'sharedlib': ['nvigi.plugin.hwi.common'],
            'includes': ['source/plugins/nvigi.hwi/common/nvigi_hwi_common.h'],
            'sources': ['plugins/nvigi.hwi/common', 'shared'],
            '3rdparty': cuptiDlls,
            'premake': 'source/plugins/nvigi.hwi/common/premake.lua' 
        },
        'plugin.hwi.cuda' : {
            'platforms': win_plat,
            'sharedlib': ['nvigi.plugin.hwi.cuda'],
            'includes': ['source/plugins/nvigi.hwi/cuda/nvigi_hwi_cuda.h'],
            'sources': ['plugins/nvigi.hwi/cuda', 'shared'],
            'externals': [cuda_ext, cig_scheduler_settings_ext, vulkan_ext],
            '3rdparty': cuda12dlls,
            'premake': 'source/plugins/nvigi.hwi/cuda/premake.lua' 
        },
        'plugin.hwi.d3d12' : {
            'platforms': win_plat,
            'sharedlib': ['nvigi.plugin.hwi.d3d12'],
            'includes': ['source/plugins/nvigi.hwi/d3d12/nvigi_hwi_d3d12.h'],
            'sources': ['plugins/nvigi.hwi/d3d12', 'shared'],
            'externals': [cig_scheduler_settings_ext],
            'premake': 'source/plugins/nvigi.hwi/d3d12/premake.lua' 
        },
        'plugin.template.generic' : {
            'platforms': win_plat,
            'sharedlib': ['nvigi.plugin.template.generic'],
            'includes': ['source/plugins/nvigi.template.generic/nvigi_template.h'],
            'sources': ['plugins/nvigi.template.generic', 'shared'],
            'premake': 'source/plugins/nvigi.template.generic/premake.lua' 
        },
        'plugin.template.inference' : {
            'platforms': win_plat,
            'sharedlib': ['nvigi.plugin.template.inference'],
            'includes': ['source/plugins/nvigi.template.inference/nvigi_template_infer.h'],
            'sources': ['plugins/nvigi.template.inference', 'shared'],
            'premake': 'source/plugins/nvigi.template.inference/premake.lua',
            'model': 'nvigi.plugin.template.inference',
            'public_models': ['{01234567-0123-0123-0123-0123456789AB}']
        },
        'plugin.template.inference.cuda' : {
            'platforms': win_plat,
            'sharedlib': ['nvigi.plugin.template.inference.cuda'],
            'includes': ['source/plugins/nvigi.template.inference.cuda/nvigi_template_infer.h'],
            'sources': ['plugins/nvigi.template.inference.cuda', 'shared'],
            'premake': 'source/plugins/nvigi.template.inference.cuda/premake.lua' 
        },
        'test' : {
            'platforms': all_plat,
            '3rdparty': cuptiDlls,
            'exes': ['nvigi.test'],
            'sources': ['tests/ai'],
            'externals': [nlohmann_json_ext, catch2_ext, vulkan_ext],
            'premake': 'source/tests/ai/premake.lua' 
        },
        'tool.utils' : {
            'platforms': all_plat,
            'exes': ['nvigi.tool.utils'],
            'sources': ['tools/utils'],
            'externals': [nlohmann_json_ext],
            'premake': 'source/tools/utils/premake.lua' 
        }
    }

    runtime_components = [
        'core.framework',
        'plugin.hwi.common',
        'plugin.hwi.cuda',
        'plugin.hwi.d3d12',
        'plugin.template.generic',
        'plugin.template.inference',
        'plugin.template.inference.cuda',
        'test',
        'tool.utils'
    ]

    runtime_source_components = [
        'core.framework',
        'plugin.template.generic',
        'plugin.template.inference',
        'plugin.template.inference.cuda'
    ]

    pdk_source_components = [
        'plugin.hwi.common',
        'plugin.hwi.cuda',
        'plugin.hwi.d3d12',
        'test',
        'tool.utils'
    ]

    dest_models=dest_root/'data/nvigi.models'
    dest_model_config=dest_root/'data/nvigi.models/configs'

    ReadonlyTree(dest_root, False)
    if args.delete_sdk:
        DeleteDirTree(args.dest)

    config_list = []
    if args.pack_debug:
        config_list += ['Debug_x64']
    if args.pack_production:
        config_list += ['Production_x64']
    if args.pack_release:
        config_list += ['Release_x64']

    if not config_list:
        config_list = ['Debug_x64', 'Production_x64', 'Release_x64']
    
    fullsdk_components = runtime_components
    source_components = runtime_source_components
    if args.config != 'runtime':
        source_components += pdk_source_components

    fullsdk_components += runtime_source_components + pdk_source_components

    # Depending upon the desired SDK, select the set of active component names
    component_names_all_plats = fullsdk_components
            
    component_names = []
    for component in component_names_all_plats:
        if args.platform in components[component]['platforms']:
            component_names.append(component)

    early_out = False
    
    # Generate the project.xml for the full, required set of third-party DLLs for the enabled components
    if args.packman_only!="":
        PackmanOnly(component_names, components, args.packman_only)
        early_out = True
    
    if args.premake_only!="":
        PremakeOnly(component_names, components, args.premake_only)
        early_out = True

    if early_out:
        sys.exit(0)

    # Scripts: Stage the copies
    scripts = ExtractCopySourcesList(components, component_names, 'scripts')
    for scr, component in scripts:
        copy_list += utils.CopySelect(src/'scripts'/scr, dest_root, component)
        if not args.source:
            for config in config_list:
                dest_bin = dest_root/f'bin/{config}'
                copy_list += utils.CopySelect(src/'scripts'/scr, dest_bin, component)

    # Models: Stage the copies
    models_dirs = ExtractCopySourcesList(components, component_names, 'public_models')
    model_config_final_dest = dest_models if args.config == 'src' else dest_model_config

    for plugin, component in models_dirs:
        model = components[component]['model']
        src_model_plugin = src/'data/nvigi.models/configs'/model
        for guid in components[component]['public_models']:
            if os.path.exists(src_model_plugin/guid):
                copy_list += utils.CopySelect(src_model_plugin/guid, model_config_final_dest, component, prefix=src/'data/nvigi.models/configs')
        src_model_plugin = src/'data/nvigi.models'/model
        for guid in components[component]['public_models']:
            if os.path.exists(src_model_plugin/guid):
                copy_list += utils.CopySelect(src_model_plugin/guid, dest_models, component, prefix=src/'data/nvigi.models')

    # Docs: Stage the copies
    docs = ExtractCopySourcesList(components, component_names, 'docs')
    dest_docs = dest_root/'docs'
    dest_docs_media = dest_docs/'media'
    for doc, component in docs:
        copy_list += utils.CopySelect(src/'docs'/doc, dest_docs, component)
    copy_list += utils.CopySelect(src/'docs/media/*.png', dest_docs_media, 'all')
    copy_list += utils.CopySelect(src/'docs/media/*.svg', dest_docs_media, 'all')

    # Build tools, configs, etc for all SDKs
    copy_list.append((src/'docs/3rd-party-licenses.md', dest_root/'docs', 'all'))
    copy_list.append((src/'LICENSE.txt', dest_root, 'all'))
    copy_list.append((src/'NOTICE.txt', dest_root, 'all'))
    copy_list.append((src/'README.md', dest_root, 'all'))

    # Lots of things we do not ship in source packs
    if args.config == 'src':
        copy_list.append((src/'.gitattributes', dest_root, 'all'))
        copy_list.append((src/'.gitignore', dest_root, 'all'))
    else:
        # DLLs (and Symbols): Stage the copies
        dlls = ExtractCopySourcesList(components, component_names, 'sharedlib')
        for config in config_list:
            dest_bin = dest_root/f'bin/{config}'
            dest_symbols = dest_root/f'symbols/{config}'
            dest_lib = dest_root/f'lib/{config}'
            for dll, component in dlls:
                copy_list.append((src/f'bin/{config}/{dll}.dll', dest_bin, component))
                copy_list.append((src/f'symbols/{config}/{dll}.pdb', dest_symbols, component))
            
            # EXEs (and Symbols): Stage the copies
            exes = ExtractCopySourcesList(components, component_names, 'exes')
            for exe, component in exes:
                copy_list.append((src/f'bin/{config}/{exe}.exe', dest_bin, component))
                copy_list.append((src/f'symbols/{config}/{exe}.pdb', dest_symbols, component))

            # Bin Extras: Stage the copies
            bin_extras = ExtractCopySourcesList(components, component_names, 'bin_extras')
            for pair, component in bin_extras:
                src_prefix = src/f'_artifacts/nvigi.{component}/{config}/{pair[0]}'
                copy_list += utils.CopySelect(src_prefix, dest_bin/pair[1], component, prefix=src_prefix)

            # libs: Stage the copies
            libs = ExtractCopySourcesList(components, component_names, 'libs')
            for lib, component in libs:
                copy_list += utils.CopySelect(src/f'lib/{config}/{lib}.lib', dest_lib, component)
                copy_list += utils.CopySelect(src/f'lib/{config}/{lib}.exp', dest_lib, component)

            # 3rd-party: Stage the copies
            exts = ExtractCopySourcesListPlatformWithDest(components, component_names, '3rdparty', args.platform, src)
            exts = exts + ExtractCopySourcesListPlatformWithDest(components, component_names, '3rdparty_private', args.platform, src)
            for ext, component in exts:
                # If the item is a tuple, it represents (src file path, destination bin subdir)
                if isinstance(ext, tuple):
                    copy_list.append((ext[0], dest_bin/ext[1], component))
                else:
                    copy_list.append((ext, dest_bin, component))

    if args.config == 'runtime':
        # Includes: Stage the copies
        includes = ExtractCopySourcesList(components, component_names, 'includes')
        dest_inc = dest_root/'include'
        for inc, component in includes:
            copy_list += utils.CopySelect(src/inc, dest_inc, component)
    else:
        copy_list.append((src/'premake/premake.utils.lua', dest_root/'premake', 'all'))
        copy_list.append((src/'premake/premake.shared_config.lua', dest_root/'premake', 'all'))
        copy_list.append((src/'premake/premake.shared_plugin.lua', dest_root/'premake', 'all'))

        copy_list.append((src/'setup.bat', dest_root, 'all'))

        copy_list.append((src/'tools/lf2crlf.ps1', dest_root/'tools', 'all'))
        copy_list.append((src/'tools/project.tools.xml', dest_root/'tools', 'all'))
        copy_list.append((src/'tools/packman/packmanconf.py', dest_root/'tools/packman', 'all'))
        copy_list.append((src/'tools/packman/packman.cmd', dest_root/'tools/packman', 'all'))
        copy_list.append((src/'tools/packman/python.bat', dest_root/'tools/packman', 'all'))
        copy_list += utils.CopyWildcards(src/'tools/packman/bootstrap', '*.*', dest_root/'tools/packman/bootstrap', 'all')

        copy_list.append((src/f'scripts/nvigi.core.framework.json', dest_root/'scripts', 'core.framework'))

        copy_list.append((src/'tools/vswhere.exe', dest_root/'tools', 'all'))
        copy_list.append((src/'tools/gitVersion.bat', dest_root/'tools', 'all'))
        copy_list.append((src/'package.bat', dest_root, 'all'))
        copy_list.append((src/'tools/packaging/package.py', dest_root/'tools/packaging', 'all'))
        copy_list.append((src/'tools/packaging/nvigi_utils.py', dest_root/'tools/packaging', 'all'))
        copy_list.append((src/'build.bat', dest_root, 'all'))
        rename_list.append((src/'tools/packman/config.sdk.packman.xml', dest_root/'tools/packman/config.packman.xml', 'all'))

    # Sources: Stage the copies for JUST the source components
    sources = ExtractCopySourcesList(components, source_components, 'runtime_sources' if args.config == 'runtime' else 'sources' )
    source_copies = []
    
    sources_skip = ExtractCopySourcesList(components, source_components, 'sources_skip')
    for source, component in sources:
        skip_list = []
        for src_skip, skip_comp in sources_skip:
            if component == skip_comp:
                skip_list.append(src_skip)
        source_copies += utils.CopySelect(src/'source'/source, dest_root, component, keep_path=True, skip_list = skip_list)
        
    copy_list += source_copies


    [copied, total, renamed] = utils.DoCopy(copy_list, rename_list, dest_root)
    

    if args.config != 'runtime':
        # Post-copy actions
        premakes = []
        for name in source_components:
            item = components[name]
            if "premake" in item:
                prem = item["premake"]
                duplicate = False
                for existing_prem in premakes:
                    if prem == existing_prem:
                        duplicate = True
                if duplicate == False:
                    premakes.append(prem)
        WritePremake(premakes, dest_root/'premake.lua', is_plugin_build=False)

        externals = []
        for name in source_components:
            item = components[name]
            if "externals" in item:
                for dep in item["externals"]:
                    duplicate = False
                    for existing_dep in externals:
                        if dep["dep"] == existing_dep["dep"]:
                            duplicate = True
                            if dep != existing_dep:
                                print(f'Conflicting packman dependencies: {dep["dep"]}')
                    if duplicate == False:
                        externals.append(dep)
        WritePackman(externals, dest_root/'project.xml')

    # Make the copied headers, scripts and source read-only to make it more obvious they should not be edited
    ReadonlyTree(dest_root, True)
    print ('-----  SDK Packaging Options ----')
    print (f'SDK SKU packaged: {args.config}')
    if args.config != 'src':
        print (f'Build configs packaged: {config_list}')
   