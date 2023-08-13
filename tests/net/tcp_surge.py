import socket
import threading
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


def send_data():
    while True:
        conn.send(("B" * 4096).encode())
        print("sent.")


def recv_data():
    while True:
        data = conn.recv(4096)
        print(f"recv: {len(data)}")


send_thread = threading.Thread(target=send_data, daemon=True)
recv_thread = threading.Thread(target=recv_data, daemon=True)

send_thread.start()
recv_thread.start()

send_thread.join()
recv_thread.join()

print("close...")
conn.close()
s.close()
