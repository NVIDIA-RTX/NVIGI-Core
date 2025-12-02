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
                           "items":[{"name":"nlohmann_json", "version":"3.10.5.1-b440a32a-windows-x86_64","platforms":"windows-x86_64"}]
                          }

catch2_ext = {"dep":"catch2", "path":"external/catch2",
                           "items":[{"name":"catch2", "version":"2.13.9-7011809a","platforms":"windows-x86_64"}]
                          }

cuda_ext = {"dep":"cuda", "path":"external/cuda", 
                           "items":[{"name":"cuda", "version":"v12.8.0-windows-x64","platforms":"windows-x86_64"}]
                          }

cig_scheduler_settings_ext = {"dep":"cig_scheduler_settings", "path":"external/cig_scheduler_settings", 
                           "items":[{"name":"cig_scheduler_settings_win64", "version":"202507021723-ac549a08-main","platforms":"windows-x86_64"}]
                          }

nvapi_ext = {"dep":"nvapi", "path":"external/nvapi", 
                           "items":[{"name":"nvapi", "version":"r575-public-windows-x86_64","platforms":"windows-x86_64"}]
                          }

agility_sdk_ext = {"dep":"agility-sdk-code", "path":"external/agility-sdk",
                            "items":[{"name":"agility-sdk-code", "version":"1.615.1-x64-windows","platforms":"windows-x86_64"}]
                    }

amd_ags_ext = {"dep":"amd-ags", "path":"external/amd-ags",
                            "items":[{"name":"amd-ags", "version":"6.2.0-x64-windows","platforms":"windows-x86_64"}]
                    }

vulkan_ext = {"dep":"vulkansdk", "path":"external/vulkanSDK",
                           "items":[{"name":"vulkansdk", "version":"1.4.321.1-windows-x86_64","platforms":"windows-x86_64"}]
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

all_plat = ['win-x64']
plat_suffixes = {'win-x64':'x64'}

all_components = {
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
        'platforms': all_plat,
        'sharedlib': ['nvigi.plugin.hwi.common'],
        'includes': ['source/plugins/nvigi.hwi/common/nvigi_hwi_common.h'],
        'sources': ['plugins/nvigi.hwi/common', 'shared'],
        '3rdparty': cuptiDlls,
        'premake': 'source/plugins/nvigi.hwi/common/premake.lua' 
    },
    'plugin.hwi.cuda' : {
        'platforms': all_plat,
        'sharedlib': ['nvigi.plugin.hwi.cuda'],
        'includes': ['source/plugins/nvigi.hwi/cuda/nvigi_hwi_cuda.h'],
        'sources': ['plugins/nvigi.hwi/cuda', 'shared'],
        'externals': [cuda_ext, cig_scheduler_settings_ext, vulkan_ext],
        '3rdparty': cuda12dlls,
        'premake': 'source/plugins/nvigi.hwi/cuda/premake.lua' 
    },
    'plugin.hwi.d3d12' : {
        'platforms': all_plat,
        'sharedlib': ['nvigi.plugin.hwi.d3d12'],
        'includes': ['source/plugins/nvigi.hwi/d3d12/nvigi_hwi_d3d12.h'],
        'sources': ['plugins/nvigi.hwi/d3d12', 'shared'],
        'externals': [cig_scheduler_settings_ext],
        'premake': 'source/plugins/nvigi.hwi/d3d12/premake.lua' 
    },
    'plugin.template.generic' : {
        'platforms': all_plat,
        'sharedlib': ['nvigi.plugin.template.generic'],
        'includes': ['source/plugins/nvigi.template.generic/nvigi_template.h'],
        'sources': ['plugins/nvigi.template.generic', 'shared'],
        'premake': 'source/plugins/nvigi.template.generic/premake.lua' 
    },
    'plugin.template.inference' : {
        'platforms': all_plat,
        'sharedlib': ['nvigi.plugin.template.inference'],
        'includes': ['source/plugins/nvigi.template.inference/nvigi_template_infer.h'],
        'sources': ['plugins/nvigi.template.inference', 'shared'],
        'premake': 'source/plugins/nvigi.template.inference/premake.lua',
        'model': 'nvigi.plugin.template.inference',
        'public_models': ['{01234567-0123-0123-0123-0123456789AB}']
    },
    'plugin.template.inference.cuda' : {
        'platforms': all_plat,
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

def ReadonlyTree(dest_root, readonly):
    readonly_files = []
    readonly_files.extend((dest_root/'include/').glob('**/*.*'))
    readonly_files.extend((dest_root/'source/').glob('**/*.*'))
    for dest_file in readonly_files:
        if not os.path.isdir(dest_file):
            os.chmod(dest_file, stat.S_IREAD if readonly else stat.S_IWRITE)

# Returns sources_list, a list of (source, component_name) tuples
def ExtractCopySourcesList(component_list, component_names, category, platform=None):
    sources_list = []
    for name in component_names:
        component = component_list[name]
        if platform != None:
            if platform not in component['platforms']:
                break
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

def ExtractPackmanItems(item, list_names, packman_items):
    for list_name in list_names:
        if list_name in item:
            for dep in item[list_name]:
                duplicate = False
                for existing_dep in packman_items:
                    if dep["dep"] == existing_dep["dep"]:
                        duplicate = True
                        if dep != existing_dep:
                            print(f'Conflicting packman dependencies: {dep["dep"]}')
                if duplicate == False:
                    packman_items.append(dep)
    return packman_items

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
        print('<project toolsVersion="6.21">', file=f)
        for dep in sorted_externals:
            print(f'\t<dependency name="{dep["dep"]}" linkPath="{dep["path"]}">', file=f)
            for item in dep["items"]:
                if "version" in item:
                    print(f'\t\t<package name="{item["name"]}" version="{item["version"]}" platforms="{item["platforms"]}"/>', file=f)
                elif "path" in item:
                    print(f'\t\t<source path="{item["path"]}" />', file=f)
            print('\t</dependency>', file=f)
        print('</project>', file=f)

def PackmanCreate(component_names, components, tags, filename):
    externals = []
    for name in component_names:
        item = components[name]
        externals = ExtractPackmanItems(item, tags, externals)
    WritePackman(externals, filename)

def WritePremake(premakes, filename):
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

def PremakeCreate(component_names, components, filename):
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
    WritePremake(premakes, filename)

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
                        dest='build_modes',action='append_const', const='Debug',
                        help=f'Package the debug config (default all configs)')
    parser.add_argument('-production', 
                        dest='build_modes',action='append_const', const='Production',
                        help=f'Package the production config (default all configs)')
    parser.add_argument('-release', 
                        dest='build_modes',action='append_const', const='Release',
                        help=f'Package the release config (default all configs)')

    parser.add_argument('-no-deletion',
                        dest='delete_sdk', action='store_false',
                        help=f'Skip deleting SDK')

    parser.add_argument('-x64',
                        dest='platforms',
                        help=f'Support x64',
                        action='append_const', const='win-x64')

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
    dest_models=dest_root/'data/nvigi.models'
    dest_model_config=dest_root/'data/nvigi.models/configs'

    if src.exists() and dest_root.exists() and os.path.samefile(dest_root, src):
        print(f'source ({src}) and destination ({dest_root}) of packaging cannot be the same directory!')
        sys.exit(-1)

    copy_list = []
    rename_list = []

    if args.platforms is None or len(args.platforms) == 0:
       args.platforms = ['win-x64']

    print (f'Prepping/Packing platforms: {args.platforms}')

    if args.build_modes is None:
        args.build_modes = ['Debug', 'Production', 'Release']

    config_list = []
    for build in args.build_modes:
        config_list += [build]

    ReadonlyTree(dest_root, False)
    if args.delete_sdk:
        utils.DeleteDirTree(args.dest)

    fullsdk_components = runtime_components
    source_components = runtime_source_components
    if args.config != 'runtime':
        source_components += pdk_source_components

    fullsdk_components += runtime_source_components + pdk_source_components

    # Depending upon the desired SDK, select the set of active component names
    component_names_all_plats = fullsdk_components
    component_names = []
    for component in component_names_all_plats:
        # check all platforms - add a component ONCE if ANY platform supports it
        for platform in args.platforms:
            if platform in all_components[component]['platforms']:
                component_names.append(component)
                break

    early_out = False
    
    # Generate the project.xml for the full, required set of third-party DLLs for the enabled components
    if args.packman_only!="":
        # We ONLY add packman packages for internal-only packages to the local build files,
        # NOT the packaged project.xml.  This is important if the items are to be distributed
        # in the click-through licensed nvigi_pack, and NOT from public packman
        PackmanCreate(component_names, all_components, ["externals", "externals_private"], args.packman_only)
        early_out = True
    
    if args.premake_only!="":
        PremakeCreate(component_names, all_components, args.premake_only)
        early_out = True

    if early_out:
        sys.exit(0)

    # Scripts: Stage the copies
    scripts = ExtractCopySourcesList(all_components, component_names, 'scripts')
    for scr, component in scripts:
        copy_list += utils.CopySelect(src/'scripts'/scr, dest_root, component)
        if not args.source:
            for plat in args.platforms:
                plat_suffix = plat_suffixes[plat]
                for config in config_list:
                    dest_bin = dest_root/f'bin/{config}_{plat_suffix}'
                    copy_list += utils.CopySelect(src/'scripts'/scr, dest_bin, component)

    # Models: Stage the copies
    models_dirs = ExtractCopySourcesList(all_components, component_names, 'public_models')
    model_config_final_dest = dest_models if args.config == 'src' else dest_model_config

    for plugin, component in models_dirs:
        model = all_components[component]['model']
        src_model_plugin = src/'data/nvigi.models/configs'/model
        for guid in all_components[component]['public_models']:
            if os.path.exists(src_model_plugin/guid):
                copy_list += utils.CopySelect(src_model_plugin/guid, model_config_final_dest, component, prefix=src/'data/nvigi.models/configs')
        src_model_plugin = src/'data/nvigi.models'/model
        for guid in all_components[component]['public_models']:
            if os.path.exists(src_model_plugin/guid):
                copy_list += utils.CopySelect(src_model_plugin/guid, dest_models, component, prefix=src/'data/nvigi.models')

    # Docs: Stage the copies
    docs = ExtractCopySourcesList(all_components, component_names, 'docs')
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

    # Sources: Stage the copies for JUST the source components
    sources = ExtractCopySourcesList(all_components, source_components, 'runtime_sources' if args.config == 'runtime' else 'sources' )
    source_copies = []

    sources_skip = ExtractCopySourcesList(all_components, source_components, 'sources_skip')
    for source, component in sources:
        skip_list = []
        for src_skip, skip_comp in sources_skip:
            if component == skip_comp:
                skip_list.append(src_skip)
        source_copies += utils.CopySelect(src/'source'/source, dest_root, component, keep_path=True, skip_list = skip_list)

    copy_list += source_copies

    # Lots of things we do not ship in source packs
    if args.config == 'src':
        copy_list.append((src/'.gitattributes', dest_root, 'all'))
        copy_list.append((src/'.gitignore', dest_root, 'all'))
    else:
        # Platform and config-specific items: Stage the copies
        for platform in args.platforms:
            plat_suffix = plat_suffixes[platform]
            dlls = ExtractCopySourcesList(all_components, component_names, 'sharedlib', platform=platform)
            exes = ExtractCopySourcesList(all_components, component_names, 'exes', platform=platform)
            bin_extras = ExtractCopySourcesList(all_components, component_names, 'bin_extras', platform=platform)
            libs = ExtractCopySourcesList(all_components, component_names, 'libs', platform=platform)
            exts = ExtractCopySourcesListPlatformWithDest(all_components, component_names, '3rdparty_private', platform, src) + ExtractCopySourcesListPlatformWithDest(all_components, component_names, '3rdparty', platform, src)

            for config in config_list:
                config_name = f'{config}_{plat_suffix}'
                dest_bin = dest_root/f'bin/{config_name}'
                dest_symbols = dest_root/f'symbols/{config_name}'
                dest_lib = dest_root/f'lib/{config_name}'

                for dll, component in dlls:
                    copy_list.append((src/f'bin/{config_name}/{dll}.dll', dest_bin, component))
                    copy_list.append((src/f'symbols/{config_name}/{dll}.pdb', dest_symbols, component))

                # EXEs (and Symbols): Stage the copies
                for exe, component in exes:
                    copy_list.append((src/f'bin/{config_name}/{exe}.exe', dest_bin, component))
                    copy_list.append((src/f'symbols/{config_name}/{exe}.pdb', dest_symbols, component))

                # Bin Extras: Stage the copies
                for pair, component in bin_extras:
                    src_prefix = src/f'_artifacts/nvigi.{component}/{config_name}/{pair[0]}'
                    copy_list += utils.CopySelect(src_prefix, dest_bin/pair[1], component, prefix=src_prefix)

                # libs: Stage the copies
                for lib, component in libs:
                    copy_list += utils.CopySelect(src/f'lib/{config_name}/{lib}.lib', dest_lib, component)
                    copy_list += utils.CopySelect(src/f'lib/{config_name}/{lib}.exp', dest_lib, component)

                # 3rd-party: Stage the copies
                for ext, component in exts:
                    # If the item is a tuple, it represents (src file path, destination bin subdir)
                    if isinstance(ext, tuple):
                        copy_list.append((ext[0], dest_bin/ext[1], component))
                    else:
                        copy_list.append((ext, dest_bin, component))

    if args.config == 'runtime':
        # Includes: Stage the copies
        includes = ExtractCopySourcesList(all_components, component_names, 'includes')
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

        if args.config != 'src':
            copy_list.append((src/'tools/packman/config.packman.xml', dest_root/'tools/packman', 'all'))

        copy_list.append((src/'tools/vswhere.exe', dest_root/'tools', 'all'))
        copy_list.append((src/'tools/gitVersion.bat', dest_root/'tools', 'all'))
        copy_list.append((src/'package.bat', dest_root, 'all'))
        copy_list.append((src/'tools/packaging/package.py', dest_root/'tools/packaging', 'all'))
        copy_list.append((src/'tools/packaging/nvigi_utils.py', dest_root/'tools/packaging', 'all'))
        copy_list.append((src/'build.bat', dest_root, 'all'))
        rename_list.append((src/'tools/packman/config.sdk.packman.xml', dest_root/'tools/packman/config.packman.xml', 'all'))

    # Perform the copies and renames based upon the staged lists
    [copied, total, renamed] = utils.DoCopy(copy_list, rename_list, dest_root)

    # Post-copy actions
    if args.config != 'runtime':
        PremakeCreate(source_components, all_components, dest_root/'premake.lua')
        PackmanCreate(source_components, all_components, ["externals"], dest_root/'project.xml')

    # Make the copied headers, scripts and source read-only to make it more obvious they should not be edited
    ReadonlyTree(dest_root, True)
    print ('-----  SDK Packaging Options ----')
    print (f'SDK SKU packaged: {args.config}')
    if args.config != 'src':
        print (f'Build configs packaged: {config_list}')
   