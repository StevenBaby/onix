import socket
import time
from net import (
    haddr,
    laddr
)

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
print(s)

s.bind((haddr, 7777))
s.connect((laddr, 6666))


while True:
    print("receivng...")
    message = s.recv(1024)
    print("recved:", message)
    s.send(bytes(f"udp ack message {int(time.time())}", encoding='ascii'))
    break

s.close()
