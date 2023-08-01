import sys
import socket
import logging
import time

logging.basicConfig(
    stream=sys.stdout,
    level=logging.DEBUG,
    format='[%(module)s:%(lineno)d] %(levelname)s %(message)s',)

logger = logging.getLogger("net")

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

print("bind...")
s.bind(('0.0.0.0', 7777))

print("listen...")
s.listen(5)

print("accept...")
conn, addr = s.accept()
logger.debug(conn)

print(addr)

time.sleep(3)

print("close...")
conn.close()
s.close()
