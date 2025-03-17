import argparse
import glob
import os
from pathlib import Path
import shutil
import stat
import sys
from functools import cmp_to_key

def ReadonlyTree(dest_root, readonly):
    readonly_files = []
    readonly_files.extend((dest_root/'include/').glob('**/*.*'))
    readonly_files.extend((dest_root/'source/').glob('**/*.*'))
    for dest_file in readonly_files:
        if not os.path.isdir(dest_file):
            os.chmod(dest_file, stat.S_IREAD if readonly else stat.S_IWRITE)

def DeleteDirTree(dir):
    # Remove the directory and all its contents
     if os.path.exists(dir):
        shutil.rmtree(dir)

def CopyIfNewer(src, dst_dir):
    if not os.path.isdir(dst_dir):
        print(f"The destination {dst_dir} is not a directory.")
        return 0

    dst_file = os.path.join(dst_dir, os.path.basename(src))

    if os.path.islink(src):
        linkto = os.readlink(src)
        os.symlink(linkto, dst_file)
    elif not os.path.exists(dst_file):
        shutil.copy2(src, dst_file)
    else:
        src_mtime = os.path.getmtime(src)
        dst_mtime = os.path.getmtime(dst_file)

        if src_mtime > dst_mtime:
            shutil.copy2(src, dst_file)
        else:
            return 0
    return 1

def PrintProgressBar(title, iteration, total, length=50):
    percent = 100 * (iteration / float(total))
    filled_length = int(length * iteration // total)
    bar = '#' * filled_length + '-' * (length - filled_length)
    print(f'\r{title}: |{bar}| {percent:.2f}% complete', end='\r')
    if iteration == total:
        print()  # Print a newline at the end

def CopyWildcards(src_path, wildcard,  dest_path):
    copy_list = []
    for src in src_path.glob(wildcard):
        if src.is_file()==False:
            print(f'{src} Does not exist!')
            sys.exit(-1)
        copy_list.append((src, dest_path))
    return copy_list

# src_path is assumed to be relative and will be appended to dest_path for the copy
# e.g.
# src_path = source/core
# dest_path = my_sdk
# will lead to a copy of the form:
# <cwd>/source/core/nvigi.api/nvigi.h -> <cwd>/my_sdk/source/core/nvigi.api/nvigi.h
def CopyTree(src_path, dest_path, prefix=""):
    copy_list = []
    if src_path.is_dir()==False:
        print(f'{src_path} Does not exist!')
        sys.exit(-1)
    for src in src_path.glob(f'**/*.*'):
        if src.is_file()==True:
            if prefix=="":
                copy_list.append((src, dest_path/src.parent))
            else:
                copy_list.append((src, dest_path/src.relative_to(prefix).parent))
    return copy_list

def CopyFile(src_path, dest_path, keep_path=False):
            if keep_path==True:
                return [(src_path, dest_path/src_path.parent)]    
            else:
                return [(src_path, dest_path)]

def CopySelect(src_path, dest_path, prefix="", keep_path=False):
    if '*' in str(src_path):
        # Walk backwards up the src path until all * wildcards are found, so we split the
        # root path from the wildcard even in cases like <path>/**/*.h
        src_dir = Path(src_path.parent)
        wildcard = src_path.name
        while '*' in str(src_dir):
            wildcard = Path(src_dir.name)/wildcard
            src_dir = src_dir.parent
        return CopyWildcards(src_dir, str(wildcard), dest_path)
    else:
        if src_path.exists():
            if src_path.is_file():
                return CopyFile(src_path, dest_path, keep_path)
            elif src_path.is_dir():
                return CopyTree(src_path, dest_path, prefix)
        else:
            print(f'{src_path} Does not exist!')
            sys.exit(-1) 
            
def CopyFileList(copy_list):
    total = len(copy_list)
    iteration = 0
    copied = 0
    for src,dest in copy_list:
        PrintProgressBar("-> copying", iteration + 1, total)
        if dest.exists()==False:
            dest.mkdir(parents=True, exist_ok=True)
        copied += CopyIfNewer(src, dest)
        iteration = iteration + 1    
    return copied, total

def LogFileLists(copy_list, file):
    with open(file, 'w') as f:
        for src,dest in copy_list:
            print(f'\t{dest}/{src.name}', file=f)

def Dedup(copy_list):
    output_list = set()
    for item in copy_list:
        output_list.add(item)
    return list(output_list)

# Sort by a[1] (dest) and then a[0] (src) if dests match
def PathPairCompare(a, b):
    cmp = 0
    # argh: (a>b)-(a<b) replacing the removed cmp()
    if str(a[1]) != str(b[1]):
        cmp = (str(a[1]) > str(b[1])) - (str(a[1]) < str(b[1]))
    else:
        cmp = (str(a[0]) > str(b[0])) - (str(a[0]) < str(b[0]))
    return cmp
    
def DoCopy(copy_list, dest_root):
    # Remove duplicates, as we often have src/dest pairs that are in multiple components
    copy_list = Dedup(copy_list)

    # Sort the lists for east reading
    sorted_copy_list = sorted(copy_list, key=cmp_to_key(PathPairCompare))

    # TODO - look for overwrite copies
    # These are cases where all of the following are true:
    # - the destinations are the same
    # - the source filenames are the same
    # - the source paths are different

    [copied, total] = CopyFileList(copy_list)
    LogFileLists(sorted_copy_list, dest_root/'manifest.txt')
    return copied, total


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


    args, unknown = parser.parse_known_args()
    
    return args

if __name__ == '__main__':
    args = parse_args()
    dest_root = Path(f'{args.dest}')

    copy_list = []
    src=Path('.')
    
    if src.exists() and dest_root.exists() and os.path.samefile(dest_root, src):
        print(f'source ({src}) and destination ({dest_root}) of packaging cannot be the same directory!')
        sys.exit(-1)

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
        
    # Simple for now.  High level, redactions

    # EVERYONE gets the license!    
    copy_list += CopySelect(src/'LICENSE.txt', dest_root)

    # A few doc dependencies are in every pack
    copy_list += CopySelect(src/'docs/media/*.png', dest_root/'docs/media')
    copy_list += CopySelect(src/'docs/media/*.svg', dest_root/'docs/media')
    
    # First, binaries get copied into all non-source packs.
    if args.config != 'src':
        # All non-source SDK configs get these:
        for config in config_list:
            copy_list += CopySelect(src/f'bin/{config}/*', dest_root/f'bin/{config}')
            if os.path.exists(src/f'lib'):
                copy_list += CopySelect(src/f'lib/{config}/*', dest_root/f'lib/{config}')
            if os.path.exists(src/'symbols'):
                copy_list += CopySelect(src/f'symbols/{config}/*', dest_root/f'symbols/{config}')
    
        # Install CUDA runtime to all configs
        cudaRt = src/'external/cuda/bin/cudart64_12.dll'
        cupti = src/'external/cuda/extras/CUPTI/lib64/cupti64_2025.1.0.dll'
        cig_settings = src/'external/cig_scheduler_settings/bin/Release_x64/cig_scheduler_settings.dll'
        if os.path.exists(cudaRt):
            for config in config_list:
                copy_list += CopySelect(cudaRt, dest_root/f'bin/{config}')
                copy_list += CopySelect(cupti, dest_root/f'bin/{config}')
                copy_list += CopySelect(cig_settings, dest_root/f'bin/{config}')

    # Next, the rest - docs, source, scripts, etc
    if args.config == 'runtime':
        # Only specific docs aimed at app devs
        copy_list += CopySelect(src/'docs/Architecture.md', dest_root/'docs')
        copy_list += CopySelect(src/'docs/GpuSchedulingForAI.md', dest_root/'docs')
        copy_list += CopySelect(src/'docs/ProgrammingGuide.md', dest_root/'docs')
        copy_list += CopySelect(src/'docs/ProgrammingGuideAI.md', dest_root/'docs')

        # CoreRuntimeSDK only adds the public headers, copied to the top level "include", NOT in-place
        copy_list += CopySelect(src/'**/nvigi*.h', dest_root/'include')
        
        # CoreRuntimeSDK only adds a few util source trees - those needed by apps
        copy_list += CopySelect(src/'source/utils/nvigi.cig_compatibility_checker/*', dest_root/'source/utils/nvigi.cig_compatibility_checker')
        copy_list += CopySelect(src/'source/utils/nvigi.dsound/*', dest_root/'source/utils/nvigi.dsound')
        copy_list += CopySelect(src/'source/utils/nvigi.wav/*', dest_root/'source/utils/nvigi.wav')
    elif args.config == 'pdk':
        # CorePDK adds pretty much everything, in-place
        copy_list += CopySelect(src/'docs/*.md', dest_root/'docs')
        copy_list += CopySelect(src/'scripts', dest_root)
        copy_list += CopySelect(src/'source', dest_root)
        copy_list += CopySelect(src/'tools/packman', dest_root)
        copy_list += CopySelect(src/'tools/gitVersion.*', dest_root/'tools')
        copy_list += CopySelect(src/'tools/lf2crlf.ps1', dest_root/'tools')
        copy_list += CopySelect(src/'tools/project.tools.xml', dest_root/'tools')
        copy_list += CopySelect(src/'tools/vswhere.exe', dest_root/'tools')
        copy_list += CopySelect(src/'build.*', dest_root)
        copy_list += CopySelect(src/'package*', dest_root)
        copy_list += CopySelect(src/'premake', dest_root)
        copy_list += CopySelect(src/'premake.lua', dest_root)
        copy_list += CopySelect(src/'project.xml', dest_root)
        copy_list += CopySelect(src/'setup.*', dest_root)
    else: # source
        # Src redacts a few items
        copy_list += CopySelect(src/'docs/*.md', dest_root/'docs')
        copy_list += CopySelect(src/'scripts', dest_root)
        copy_list += CopySelect(src/'source', dest_root)
        copy_list += CopySelect(src/'tools/packman', dest_root)
        copy_list += CopySelect(src/'tools/gitVersion.bat', dest_root/'tools')
        copy_list += CopySelect(src/'tools/lf2crlf.ps1', dest_root/'tools')
        copy_list += CopySelect(src/'tools/project.tools.xml', dest_root/'tools')
        copy_list += CopySelect(src/'tools/vswhere.exe', dest_root/'tools')
        copy_list += CopySelect(src/'build.bat', dest_root)
        copy_list += CopySelect(src/'package.bat', dest_root)
        copy_list += CopySelect(src/'package.py', dest_root)
        copy_list += CopySelect(src/'premake', dest_root)
        copy_list += CopySelect(src/'premake.lua', dest_root)
        copy_list += CopySelect(src/'project.xml', dest_root)
        copy_list += CopySelect(src/'setup.bat', dest_root)

        if args.config == 'src':
            copy_list += CopySelect(src/'README.md', dest_root)
        
        # Source pack adds gitignore/gitattrib
        copy_list += CopySelect(src/'.gitattributes', dest_root)
        copy_list += CopySelect(src/'.gitignore', dest_root)

    [copied, total] = DoCopy(copy_list, dest_root)
    

    if args.config == 'src':
        os.remove(f'{dest_root}/tools/packman/config.packman.xml')
        shutil.copy2(f'{dest_root}/tools/packman/config.sdk.packman.xml', f'{dest_root}/tools/packman/config.packman.xml')
        os.remove(f'{dest_root}/tools/packman/config.sdk.packman.xml')

    # Make the copied headers, scripts and source read-only to make it more obvious they should not be edited
    ReadonlyTree(dest_root, True)
    print ('-----  SDK Packaging Options ----')
    print (f'SDK SKU packaged: {args.config}')
    if args.config != 'src':
        print (f'Build configs packaged: {config_list}')
   