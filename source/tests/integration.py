#!/usr/bin/env python3
"""Short runtime checks. Requires only Python 3 on the development/test machine."""
import pathlib,sys,os,shutil,tempfile,subprocess,socket,time,urllib.request,json
from ws import WS
bundle=pathlib.Path(sys.argv[1]).resolve() if len(sys.argv)>1 else pathlib.Path(__file__).resolve().parents[2]
env=dict(os.environ,QT_QPA_PLATFORM='offscreen')
with tempfile.TemporaryDirectory(prefix='subtítulos & prueba ',dir=os.environ.get('TEST_WORKDIR')) as tmp:
    root=pathlib.Path(tmp)
    for name in ('bin','lib','plugins','resources','models'):
        shutil.copytree(bundle/name,root/name,copy_function=os.link if name=='models' else shutil.copy2)
    shutil.copy2(bundle/'Iniciar.sh',root/'Iniciar.sh');(root/'state').mkdir()
    reserve=socket.socket();reserve.bind(('127.0.0.1',0));port=reserve.getsockname()[1];reserve.close()
    (root/'state/settings.ini').write_text(f'[General]\nport={port}\n')
    launch=[str(root/'Iniciar.sh')]
    check=subprocess.run(launch+['--self-test'],env=env,capture_output=True,text=True,timeout=10)
    assert check.returncode==0,(check.stdout,check.stderr)
    generated=(root/'state/fonts.conf').read_text();assert '&amp;' in generated
    log=open(root/'test.log','w')
    child=subprocess.Popen(launch+['--headless','--quit-after','8'],env=env,stdout=log,stderr=log)
    try:
        for _ in range(50):
            try:
                data=json.load(urllib.request.urlopen(f'http://127.0.0.1:{port}/state',timeout=.3));break
            except OSError:time.sleep(.1)
        else:raise AssertionError('HTTP server did not start')
        assert data['text']=='' and data['style']['lines']==3
        html=urllib.request.urlopen(f'http://127.0.0.1:{port}/obs').read().decode()
        assert 'background:transparent' in html and '__WS_PORT__' not in html
        assert f'ws://127.0.0.1:{port}/ws' in html
        assert len(urllib.request.urlopen(f'http://127.0.0.1:{port}/font.ttf').read())>700000
        peer=WS(port);snapshot=peer.read();assert snapshot['sequence']>=1 and snapshot['text']=='';peer.close()
        duplicate=subprocess.run(launch+['--self-test'],env=env,capture_output=True,text=True,timeout=5)
        assert duplicate.returncode==2,duplicate
        print(json.dumps({'relocation':'spaces, accents and ampersand','offline_self_test':True,'http':True,'websocket':True,'font_bundled':True,'single_instance':True}))
    finally:
        child.terminate();child.wait(timeout=20);log.close()
    (root/'state').chmod(0o555)
    try:
        denied=subprocess.run(launch+['--self-test'],env=env,capture_output=True,text=True,timeout=5)
        assert denied.returncode!=0 and 'escritura' in denied.stderr
    finally:(root/'state').chmod(0o755)
print('PASS')
