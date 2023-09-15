import time
import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

while 1:
    try:
        dt = input("Enter DT: ")
        sock.sendto(struct.pack("d", dt), ("127.0.0.1",7532))
    except:
        sock.close()
        break
        
        
