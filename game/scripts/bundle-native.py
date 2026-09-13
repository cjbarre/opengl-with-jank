#!/usr/bin/env python3
"""Stage a linked executable and its native dependencies, without touching build outputs."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def output(*args):
    return subprocess.check_output(args, text=True)


def run(*args):
    subprocess.run(args, check=True)


def mac_rpaths(binary):
    return re.findall(r'cmd LC_RPATH\n.*?\n\s*path (.*?) \(offset', output('otool', '-l', str(binary)))


def mac_deps(binary):
    return [line.strip().split(' (compatibility')[0]
            for line in output('otool', '-L', str(binary)).splitlines()[1:]]


def expand(path, loader, executable):
    return Path(path.replace('@loader_path', str(loader.parent))
                    .replace('@executable_path', str(executable.parent)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--name')
    args = parser.parse_args()
    source = args.executable.resolve()
    name = args.name or source.name
    dest = args.destination.resolve()
    lib_dir = dest / 'lib' / name
    binary = dest / 'bin' / name
    lib_dir.mkdir(parents=True, exist_ok=True)
    binary.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, binary)
    mac = sys.platform == 'darwin'
    queue = [(source, binary)]
    copied = {}
    exe_rpaths = mac_rpaths(source) if mac else []
    for original, staged in queue:
        if mac:
            rpaths = mac_rpaths(original) + exe_rpaths
            deps = mac_deps(original)
        else:
            listing = output('ldd', str(original))
            if 'not found' in listing:
                raise RuntimeError(listing)
            deps = re.findall(r'^\s*\S+ => (/\S+)', listing, re.M)
        for dep in deps:
            leaf = Path(dep).name
            if mac:
                if dep.startswith(('/usr/lib/', '/System/Library/')):
                    continue
                if dep.startswith('@rpath/'):
                    candidates = [expand(r, original, source) / dep[len('@rpath/'):]
                                  for r in rpaths]
                else:
                    candidates = [expand(dep, original, source)]
                resolved = next((p.resolve() for p in candidates if p.is_file()), None)
                if resolved is None:
                    raise RuntimeError(f'Cannot resolve {dep} from {original}')
                if resolved == original.resolve():  # dylib install ID
                    continue
            else:
                # libc and graphics drivers must come from the host OS.
                if re.match(r'lib(c|m|pthread|dl|rt|resolv|util|nss_.*)\.so', leaf) or re.match(r'lib(GL|GLX[^.]*|GLdispatch|GLES[^.]*|EGL|glapi|drm[^.]*)\.so', leaf):
                    continue
                resolved = Path(dep).resolve()
            if leaf in copied and copied[leaf] != resolved:
                raise RuntimeError(f'Conflicting libraries named {leaf}')
            if leaf not in copied:
                copied[leaf] = resolved
                target = lib_dir / leaf
                shutil.copy2(resolved, target)
                target.chmod(target.stat().st_mode | 0o200)
                queue.append((resolved, target))
            if mac:
                run('install_name_tool', '-change', dep, '@rpath/' + leaf, str(staged))
        if mac:
            for rpath in set(mac_rpaths(staged)):
                run('install_name_tool', '-delete_rpath', rpath, str(staged))
            relative = f'@executable_path/../lib/{name}' if staged == binary else '@loader_path'
            run('install_name_tool', '-add_rpath', relative, str(staged))
            if staged != binary:
                run('install_name_tool', '-id', '@rpath/' + staged.name, str(staged))
        else:
            run('patchelf', '--set-rpath', f'$ORIGIN/../lib/{name}' if staged == binary else '$ORIGIN', str(staged))
    if mac:
        for _, staged in queue:
            run('codesign', '--force', '--sign', '-', str(staged))
    print(f'Staged {name} and {len(copied)} native libraries in {dest}')


if __name__ == '__main__':
    main()
