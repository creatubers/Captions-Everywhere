#!/usr/bin/env python3
"""Package a Jammy build. Run in the same Ubuntu 22.04 environment as the build.
Usage: python3 scripts/package.py BUILD_DIR OUTPUT_DIR
"""
import pathlib, subprocess, shutil, re, sys, json
build=pathlib.Path(sys.argv[1]).resolve(); dest=pathlib.Path(sys.argv[2]).resolve()
if (dest/'bin/captions-everywhere').exists():
    raise SystemExit('Usa un destino nuevo para empaquetar; no se sobrescribe una aplicación que puede estar en uso.')
for d in ('bin','lib','plugins','resources','licenses/ubuntu'): (dest/d).mkdir(parents=True,exist_ok=True)
shutil.copy2(build/'captions-everywhere',dest/'bin/captions-everywhere')
for lib in build.rglob('*.so*'):
    if lib.is_file(): shutil.copy2(lib.resolve(),dest/'lib'/lib.name)
plugins=pathlib.Path('/usr/lib/x86_64-linux-gnu/qt6/plugins')
for category in ('platforms','wayland-shell-integration','wayland-decoration-client','wayland-graphics-integration-client','xcbglintegrations'):
    if (plugins/category).exists(): shutil.copytree(plugins/category,dest/'plugins'/category,dirs_exist_ok=True)
# Resolve the complete ELF dependency closure in the build environment.
queue=[dest/'bin/captions-everywhere',*list((dest/'lib').glob('*.so*')),*list((dest/'plugins').rglob('*.so'))]
seen=set()
env=dict(__import__('os').environ,LD_LIBRARY_PATH=str(dest/'lib'))
while queue:
    file=queue.pop()
    if str(file) in seen:continue
    seen.add(str(file))
    result=subprocess.run(['ldd',str(file)],env=env,text=True,capture_output=True)
    if 'not found' in result.stdout: raise SystemExit(result.stdout)
    for path in re.findall(r'(?:=>\s+)?(/[^\s]+)',result.stdout):
        src=pathlib.Path(path)
        if src.is_file() and not (dest/'lib'/src.name).exists():
            target=dest/'lib'/src.name;shutil.copy2(src.resolve(),target);queue.append(target)
# Libraries loaded dynamically by glibc or Qt.
for name in ('libnss_files.so.2','libnss_dns.so.2','libresolv.so.2'):
    src=pathlib.Path('/lib/x86_64-linux-gnu')/name
    if src.exists():shutil.copy2(src.resolve(),dest/'lib'/name)
shutil.copy2('/lib64/ld-linux-x86-64.so.2',dest/'lib/ld-linux-x86-64.so.2')
shutil.copy2('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',dest/'resources/DejaVuSans.ttf')
shutil.copytree('/usr/share/X11/xkb',dest/'resources/xkb',dirs_exist_ok=True)
for copyright in pathlib.Path('/usr/share/doc').glob('*/copyright'):
    target=dest/'licenses/ubuntu'/copyright.parent.name
    target.mkdir(exist_ok=True);shutil.copy2(copyright.resolve(),target/'copyright')
shutil.copytree('/usr/share/common-licenses',dest/'licenses/common',dirs_exist_ok=True)
versions=subprocess.check_output(['dpkg-query','-W','-f=${binary:Package}\t${Version}\n'],text=True)
(dest/'licenses/ubuntu-package-versions.txt').write_text(versions)
for file in [dest/'bin/captions-everywhere',*list((dest/'lib').glob('*.so*'))]:
    subprocess.run(['strip','--strip-unneeded',str(file)],check=True)
subprocess.run(['patchelf','--set-rpath','$ORIGIN/../lib',str(dest/'bin/captions-everywhere')],check=True)
for pattern in ('libggml*.so*','libwhisper*.so*','libsherpa*.so*','libonnxruntime*.so*'):
    for file in (dest/'lib').glob(pattern):
        subprocess.run(['patchelf','--set-rpath','$ORIGIN',str(file)],check=True)
(dest/'bin/qt.conf').write_text('[Paths]\nPrefix=..\nPlugins=plugins\nLibraries=lib\n')
print('Packaged',dest)
