import time
import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

lat = 42.3396132514
lon = -71.10681816
h = 100

sock.sendto(struct.pack("ddd", lat, lon, h), ("127.0.0.1",7533));
input("Enter to start: ")

while 1:
    try:
        # READ A NEW PAIR OF LAT AND LON FROM A FILE
        sock.sendto(struct.pack("ddd", lat, lon, h), ("127.0.0.1",7533));
        time.sleep(0.1)
    except:
        sock.close()
        break
        
        
