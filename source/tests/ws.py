"""Small RFC6455 client used only by integration checks (Python standard library)."""
import socket,base64,hashlib,os,json,struct
class WS:
    def __init__(self,port,path='/ws'):
        self.sock=socket.create_connection(('127.0.0.1',port),timeout=8)
        self.file=self.sock.makefile('rb')
        key=base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall((f'GET {path} HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n').encode())
        assert b'101' in self.file.readline()
        headers={}
        while True:
            line=self.file.readline().strip()
            if not line:break
            name,value=line.split(b':',1);headers[name.lower()]=value.strip()
        expected=base64.b64encode(hashlib.sha1((key+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest())
        assert headers[b'sec-websocket-accept']==expected
    def read(self):
        while True:
            h=self.file.read(2)
            if len(h)!=2:raise EOFError('WebSocket closed')
            code=h[0]&15;n=h[1]&127
            if n==126:n=struct.unpack('!H',self.file.read(2))[0]
            if n==127:n=struct.unpack('!Q',self.file.read(8))[0]
            mask=self.file.read(4) if h[1]&128 else None
            body=self.file.read(n)
            if mask:body=bytes(c^mask[i%4] for i,c in enumerate(body))
            if code==8:raise EOFError('WebSocket closed')
            if code==9:self.send_raw(body,10);continue
            if code==1:return json.loads(body)
    def send_raw(self,body,code=1):
        mask=os.urandom(4);n=len(body);header=bytes([0x80|code])
        if n<126:header+=bytes([0x80|n])
        elif n<65536:header+=bytes([0x80|126])+struct.pack('!H',n)
        else:header+=bytes([0x80|127])+struct.pack('!Q',n)
        self.sock.sendall(header+mask+bytes(c^mask[i%4] for i,c in enumerate(body)))
    def send(self,data):self.send_raw(json.dumps(data).encode())
    def close(self):self.file.close();self.sock.close()
