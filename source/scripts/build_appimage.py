#!/usr/bin/env python3
"""Build a self-contained type-2 AppImage and zsync metadata from a fresh bundle."""
import argparse,datetime,hashlib,json,os,pathlib,re,shutil,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--bundle',required=True);p.add_argument('--output',required=True);p.add_argument('--appimagetool',required=True);p.add_argument('--runtime',required=True);p.add_argument('--repository',default='creatubers/Captions-Everywhere');args=p.parse_args()
bundle=pathlib.Path(args.bundle).resolve();out=pathlib.Path(args.output).resolve();out.mkdir(parents=True,exist_ok=True)
source=pathlib.Path(__file__).resolve().parents[1]
version=re.search(r'VERSION\s+([0-9.]+)',(source/'CMakeLists.txt').read_text()).group(1)
if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+',args.repository):raise SystemExit('Invalid GitHub repository')
owner,repo=args.repository.split('/')
filename=f'Captions-Everywhere-{version}-x86_64.AppImage'
update=f'gh-releases-zsync|{owner}|{repo}|latest|Captions-Everywhere-*-x86_64.AppImage.zsync'
for required in ['bin/captions-everywhere','models/parakeet/encoder.int8.onnx','resources/logo.png']:
 if not (bundle/required).is_file():raise SystemExit(f'Missing bundle asset: {required}')
with tempfile.TemporaryDirectory(prefix='captions-appdir-',dir=os.environ.get('CE_BUILD_TMP')) as tmp:
 appdir=pathlib.Path(tmp)/'Captions-Everywhere.AppDir';appdir.mkdir()
 # Runtime files are copied physically: never modify mapped/shared ELF inodes.
 for name in ('bin','lib','plugins','resources','licenses'):
  shutil.copytree(bundle/name,appdir/name,copy_function=shutil.copy2)
 # Models are immutable and only read during packing. Reflink/hardlink is safe here.
 shutil.copytree(bundle/'models',appdir/'models',copy_function=os.link)
 for name in ('Iniciar.sh','LEEME.md','LICENSE'):
  shutil.copy2(bundle/name,appdir/name)
 shutil.copy2(source/'appimage/AppRun',appdir/'AppRun');(appdir/'AppRun').chmod(0o755)
 (appdir/'captions_everywhere.desktop').write_text((source/'appimage/captions_everywhere.desktop.in').read_text().replace('@VERSION@',version))
 shutil.copy2(bundle/'resources/logo.png',appdir/'captions-everywhere.png')
 (appdir/'.DirIcon').symlink_to('captions-everywhere.png')
 meta=appdir/'usr/share/metainfo';meta.mkdir(parents=True)
 epoch=int(os.environ.get('SOURCE_DATE_EPOCH','0'))
 date=datetime.datetime.fromtimestamp(epoch or 1792792800,datetime.timezone.utc).date().isoformat()
 (meta/'io.github.creatubers.CaptionsEverywhere.metainfo.xml').write_text((source/'appimage/io.github.creatubers.CaptionsEverywhere.metainfo.xml.in').read_text().replace('@VERSION@',version).replace('@DATE@',date))
 (appdir/'BUILD-INFO.json').write_text(json.dumps({'name':'Captions Everywhere','version':version,'repository':args.repository,'updateInformation':update,'sourceRevision':os.environ.get('CE_SOURCE_REVISION','local')},indent=2)+'\n')
 subprocess.run(['desktop-file-validate',str(appdir/'captions_everywhere.desktop')],check=True)
 env=dict(os.environ,ARCH='x86_64',VERSION=version,APPIMAGE_EXTRACT_AND_RUN='1')
 cmd=[str(pathlib.Path(args.appimagetool).resolve()),'--runtime-file',str(pathlib.Path(args.runtime).resolve()),'--comp','zstd','--mksquashfs-opt=-Xcompression-level','--mksquashfs-opt=8','--mksquashfs-opt=-processors','--mksquashfs-opt='+os.environ.get('BUILD_JOBS','2'),'-u',update,'--file-url',f'https://github.com/{args.repository}/releases/download/v{version}/{filename}',str(appdir),str(out/filename)]
 subprocess.run(cmd,env=env,check=True)
 (out/filename).chmod(0o755)
 if not (out/(filename+'.zsync')).is_file():raise SystemExit('appimagetool did not create the zsync file')
 for path in [out/filename,out/(filename+'.zsync')]:
  with path.open('rb') as stream:digest=hashlib.file_digest(stream,'sha256').hexdigest() if hasattr(hashlib,'file_digest') else hashlib.sha256(stream.read()).hexdigest()
  with (out/'SHA256SUMS').open('a') as sums:sums.write(digest+'  '+path.name+'\n')
 print(json.dumps({'file':str(out/filename),'version':version,'updateInformation':update}))
