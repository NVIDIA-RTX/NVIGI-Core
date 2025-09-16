import os
from functools import cmp_to_key
from pathlib import Path
import shutil
import stat

# copy_list is a list of (source, dest) tuples
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

# rename_list is a list of (source, dest) tuples
def RenameFileList(rename_list):
    for src,dest in rename_list:
        shutil.copyfile(src, dest)        
    
def LogFileLists(copy_list, rename_list, file):
    with open(file, 'w') as f:
        for src,dest in copy_list:
            print(f'\t{dest}/{src.name}', file=f)
        for src,dest in rename_list:
            print(f'\t{dest}', file=f)

# copy_list is a list of (source, dest) tuples
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
    
def DoCopy(copy_list_with_components, rename_list_with_components, dest_root):
    # strip the components names, so the tuple (src, dest, component_name) become (src, dest)
    copy_list = []
    for src, dest, component in copy_list_with_components:
        copy_list.append((src, dest))

    rename_list = []
    for src, dest, component in rename_list_with_components:
        rename_list.append((src, dest))
        
    # Remove duplicates, as we often have src/dest pairs that are in multiple components
    copy_list = Dedup(copy_list)
    rename_list = Dedup(rename_list)

    # Sort the lists for east reading
    sorted_copy_list = sorted(copy_list, key=cmp_to_key(PathPairCompare))
    sorted_rename_list = sorted(rename_list, key=cmp_to_key(PathPairCompare))

    # TODO - look for overwrite copies
    # These are cases where all of the following are true:
    # - the destinations are the same
    # - the source filenames are the same
    # - the source paths are different

    [copied, total] = CopyFileList(copy_list)
    RenameFileList(rename_list)
    LogFileLists(sorted_copy_list, sorted_rename_list, dest_root/'manifest.txt')
    return copied, total, len(rename_list)

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

def DeleteDirTree(dir):
    # Remove the directory and all its contents
     if os.path.exists(dir):
        # we see some read-only files, so we need to fix those before we can delete
        for dirpath, dirnames, filenames in os.walk(dir):
            os.chmod( dirpath, stat.S_IWRITE )
            for filename in filenames:
                os.chmod(os.path.join(dirpath, filename), stat.S_IWRITE)
        shutil.rmtree(dir)    

# returns a list of (src, dest, component_name) tuples
def CopyWildcards(src_path, wildcard,  dest_path, component, skip_list = []):
    copy_list = []
    for src in src_path.glob(wildcard):
        if src.is_file()==False:
            print(f'{src} Does not exist!')
            sys.exit(-1)
        skip = False
        for skip_path in skip_list:
            if skip_path.name in src.name:
                skip = True
                continue
        if skip == False:
            copy_list.append((src, dest_path, component))
    return copy_list

# returns a list of (src, dest, component_name) tuples
# src_path is assumed to be relative and will be appended to dest_path for the copy
# e.g.
# src_path = source/plugins
# dest_path = my_sdk
# will lead to a copy of the form:
# <cwd>/source/plugins/nvigi.aip/nvigi_aip.h -> <cwd>/my_sdk/source/plugins/nvigi.aip/nvigi_aip.h
def CopyTree(src_path, dest_path, component, prefix="", skip_list = []):
    copy_list = []
    if src_path.is_dir()==False:
        print(f'{src_path} Does not exist!')
        sys.exit(-1)
    for src in src_path.glob(f'**/*.*'):
        if src.is_file()==True:
            skip = False
            for skip_path in skip_list:
                if str(Path(skip_path)) in str(src):
                    skip = True
                    continue
            if skip == False:
                if prefix=="":
                    copy_list.append((src, dest_path/src.parent, component))
                else:
                    copy_list.append((src, dest_path/src.relative_to(prefix).parent, component))
    return copy_list

# returns a list of (src, dest, component_name) tuples
def CopyFile(src_path, dest_path, component, keep_path=False, skip_list = []):
    for skip_path in skip_list:
        if skip_path in src_path:
            return []
    if keep_path==True:
        return [(src_path, dest_path/src_path.parent, component)]    
    else:
        return [(src_path, dest_path, component)]

# returns a list of (src, dest, component_name) tuples
def CopySelect(src_path, dest_path, component, prefix="", keep_path=False, skip_list = []):
    if '*' in str(src_path):
        # Walk backwards up the src path until all * wildcards are found, so we split the
        # root path from the wildcard even in cases like <path>/**/*.h
        src_dir = Path(src_path.parent)
        wildcard = src_path.name
        while '*' in str(src_dir):
            wildcard = Path(src_dir.name)/wildcard
            src_dir = src_dir.parent
        return CopyWildcards(src_dir, str(wildcard), dest_path, component, skip_list = skip_list)
    else:
        if src_path.exists():
            if src_path.is_file():
                return CopyFile(src_path, dest_path, component, keep_path, skip_list = skip_list)
            elif src_path.is_dir():
                return CopyTree(src_path, dest_path, component, prefix, skip_list = skip_list)
        else:
            print(f'{src_path} Does not exist!')
            sys.exit(-1) 
            

