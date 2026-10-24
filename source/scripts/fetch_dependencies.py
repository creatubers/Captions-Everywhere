#!/usr/bin/env python3
"""Fetch pinned build inputs; never used by the running application."""
import argparse,hashlib,json,os,pathlib,shutil,tarfile,tempfile,urllib.request
p=argparse.ArgumentParser();p.add_argument('--cache',required=True);p.add_argument('--bundle',required=True);p.add_argument('--tools',required=True);args=p.parse_args()
source=pathlib.Path(__file__).resolve().parents[1];lock=json.loads((source/'dependencies.lock.json').read_text())
cache=pathlib.Path(args.cache).resolve();cache.mkdir(parents=True,exist_ok=True)
bundle=pathlib.Path(args.bundle).resolve();(bundle/'models').mkdir(parents=True,exist_ok=True)
tools=pathlib.Path(args.tools).resolve();tools.mkdir(parents=True,exist_ok=True)
def digest(path):
 h=hashlib.sha256()
 with path.open('rb') as stream:
  for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
def download(item):
 target=cache/item['sha256']
 if target.exists() and digest(target)==item['sha256']:return target
 temporary=target.with_suffix('.partial')
 request=urllib.request.Request(item['url'],headers={'User-Agent':'Captions-Everywhere-build/1.3'})
 with urllib.request.urlopen(request,timeout=90) as response,temporary.open('wb') as file:shutil.copyfileobj(response,file)
 if digest(temporary)!=item['sha256']:raise SystemExit('Checksum mismatch for '+item['url'])
 os.replace(temporary,target);return target
def extract(archive,folder):
 with tarfile.open(archive) as tar:
  for member in tar.getmembers():
   destination=(folder/member.name).resolve()
   if not destination.is_relative_to(folder.resolve()):raise SystemExit('Unsafe archive path')
   if member.issym() or member.islnk():
    linked=(destination.parent/member.linkname).resolve()
    if not linked.is_relative_to(folder.resolve()):raise SystemExit('Unsafe archive link')
  tar.extractall(folder)
def copy(source_file,destination):
 destination.parent.mkdir(parents=True,exist_ok=True)
 if destination.exists() and digest(source_file)==digest(destination):return
 temporary=destination.with_name(destination.name+'.partial');shutil.copy2(source_file,temporary);os.replace(temporary,destination)
with tempfile.TemporaryDirectory(prefix='captions-deps-') as tmp:
 tmp=pathlib.Path(tmp)
 for name in ('sherpa','parakeet'):
  unpack=tmp/name;unpack.mkdir();extract(download(lock[name]),unpack)
  if name=='sherpa':
   directory=next(unpack.rglob('libsherpa-onnx-c-api.so')).parent
   for file in directory.glob('*.so*'):copy(file.resolve(),source/'vendor/sherpa/lib'/file.name)
  else:
   directory=next(unpack.rglob('encoder.int8.onnx')).parent
   for name in ('encoder.int8.onnx','decoder.int8.onnx','joiner.int8.onnx','tokens.txt','README.md'):
    if (directory/name).exists():copy(directory/name,bundle/'models/parakeet'/name)
for item in lock['files']:copy(download(item),bundle/item['file'])
for name,target in (('appimagetool','appimagetool.AppImage'),('appimage_runtime','runtime-x86_64')):
 copy(download(lock[name]),tools/target);(tools/target).chmod(0o755)
print('Pinned dependencies ready')
