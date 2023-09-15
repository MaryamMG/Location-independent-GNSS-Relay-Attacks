import pynmea2
import time
import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

lat = 42.349337020
lon = -71.088170584
h = 100

sock.sendto(struct.pack("ddd", lat, lon, h), ("127.0.0.1",7533));
# input("Enter to start: ")

try:
    with open('path.txt') as f:
        for line in f:
            # print(line)
            content = pynmea2.parse(line)
            lat = pynmea2.nmea_utils.dm_to_sd(content.lat)
            lon = (pynmea2.nmea_utils.dm_to_sd(content.lon)) * -1
            print(lat)
            print(lon)
            sock.sendto(struct.pack("ddd", lat, lon, h), ("127.0.0.1",7533))
            time.sleep(0.1)
except:
    sock.close()
     


