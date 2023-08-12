import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

print("bind...")
s.bind(('0.0.0.0', 7777))

print("listen...")
s.listen(5)

print("accept...")
conn, addr = s.accept()
print(conn)
print(addr)

while True:
    data = conn.recv(1024)
    print("recv:", data.decode())

print("close...")
conn.close()
s.close()
