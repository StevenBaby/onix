import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.connect(('192.168.111.33', 6666))

data = s.recv(1024)
print(data.decode())

msg = f"hello tcp server {time.time()}"
s.send(msg.encode())

time.sleep(1)
s.close()
