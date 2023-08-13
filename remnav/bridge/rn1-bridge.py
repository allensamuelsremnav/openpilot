import json
import time
import socket
import threading
import logging
import argparse
import sys
from netifaces import ifaddresses, AF_INET, interfaces
import math
import psutil

#
# Parse arguments
#
parser = argparse.ArgumentParser()
parser.add_argument('-station_ip',help="IP Address of operator station", default="127.0.0.1")
parser.add_argument('-station_port', help="Port number of operator station", type=int, default=6002)
parser.add_argument('-i', '-interfaces', metavar='InterfaceName', type=str, nargs='+',
                    help='Names of local NIC interfaces')
args = parser.parse_args()

vehicle_address = ('127.0.0.1', 6379)

def get_ip_cross(interface: str) -> str:
    """
    Cross-platform solution that should work under Linux, macOS and
    Windows.    
    """
    return ifaddresses(interface)[AF_INET][0]['addr']

station_address = (args.station_ip, args.station_port)

#
# rn1-bridge
#
def make_socket_for_interface(interface, bindto = None):
    if not hasattr(socket,'SO_BINDTODEVICE') :
        socket.SO_BINDTODEVICE = 25
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)    
#    result = sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, interface.encode('utf-8') + '\0'.encode('utf-8'))
#    assert result == 0
    if bindto:
        sock.bind(bindto)
    return sock

# List of remote interfaces: list of tuples ('ipaddr',port)
remote_interfaces = ()

last_rcv_msg_time = time.time()
last_rcv_timestamp = 0

def make_message_for_index(index, msg):
    return chr(index).encode('utf-8') + msg.encode('utf-8')

def make_beacon_for_index(index):
    return make_message_for_index(index,'{"class":"TRAJECTORY_APPLICATION","trajectory":0,"applied":0,"log":0}\n')

vehicle_to_bridge = None

class VehicleToBridge:
    def __init__(self):
        self.run = True
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.thread = threading.Thread(target=self.runner)
        self.thread.start()

    def stop(self):
        self.run = False
        self.socket.close()
        self.thread.join()

    def runner(self):
        logging.info(f"Waiting to connect to vehicle at {vehicle_address}")
        while self.run:
            try:
                self.socket.connect(vehicle_address)
            except ConnectionRefusedError:
                continue
            except socket.timeout:
                continue
            logging.info(f"Established connection to vehicle from address {self.socket.getpeername()}")
            while self.run:
                try:
                    buffer = self.socket.recv(1024).decode('utf-8')
                    for msg in buffer.split('\n'):
                        if msg.startswith('<'):
                            tag = msg[1:].split('>')[0]
                            logging.info(f"Received Vehicle Tag: {tag}")
                            vm = f'{"class":"TRAJECTORY_APPLICATION","trajectory":{tag},"applied":{int(time.time())},"log":0}'
                            StationToBridge.broadcast_reply(vm)
                        else:
                            logging.info(f"Bad message from vehicle ignored: {msg}")
                except socket.timeout:
                    continue
                except ConnectionAbortedError:
                    break


class StationToBridge:
    '''One per interface'''
    instances = {}
    def __init__(self, index, interface, port):
        self.index = index
        self.interface = interface
        HEARTBEAT_TIME = 5.0 # seconds
        global last_rcv_msg_time, last_rcv_timestamp
        #self.ip = get_ip_cross(interface)
        self.ip = "127.0.0.1"
        self.port = port
        logging.info(f"Found interface {interface} with IP Address {self.ip}, now listening on port {port}")
        self.socket = make_socket_for_interface(interface, bindto=(self.ip,port))
        self.socket.settimeout(HEARTBEAT_TIME)
        self.thread = threading.Thread(target=self.listener)
        StationToBridge.instances[index] = self
        self.thread.run()

    def broadcast_reply(m):
        logging.info(f"Sending through all {len(StationToBridge.instances)} stations: {m}")
        for s in StationToBridge.instances.values():
            s.socket.sendto(make_message_for_index(s.index, m), station_address)

    def listener(self):
        global last_rcv_timestamp, vehicle_to_bridge
        logging.info(f"Sending beacon from {self.interface} to {station_address}")
        b = make_beacon_for_index(self.index)
        print("Bytes are: ",b)
        bytes = self.socket.sendto(b, station_address)
        logging.info(f"Sent {bytes} bytes")
        while True:
            try:
                logging.info(f"Waiting for response on {self.interface}")
                message, address = self.socket.recvfrom(1024)
                logging.info(f"Received on interface {self.interface} from {address} msg: {message}")
                msg = json.loads(message)
                if msg['class'] == "TRAJECTORY":
                    request_timestamp = int(msg['requested'])
                    if request_timestamp > last_rcv_timestamp:
                        last_rcv_timestamp = request_timestamp
                        last_rcv_msg_time = time.time()
                        if not vehicle_to_bridge:
                            logging.info(f"Opening connection to vehicle")
                            vehicle_to_bridge = VehicleToBridge()
                        StationToBridge.broadcast_reply(self.generate_reply(msg))
                    else:
                        logging.info(f"Discarding message to {self.interface}, duplicate")
            except socket.timeout:
                if vehicle_to_bridge:
                    logging.info("Timeout on interface {self.interface}, closing vehicle connection")
                    vehicle_to_bridge.stop()
                    vehicle_to_bridge = None

                logging.info(f"Sending beacon from {self.interface} to {station_address}")
                self.socket.sendto(make_beacon_for_index(self.index), station_address)

    def generate_reply(self, msg):
        curvature = msg['curvature']
        if math.isinf(curvature):
            steering = 0
        else:
            '''Compute steering from Bicycle Model'''
            radius = 1 / curvature
        reply = {
            'class':'TRAJECTORY_ADJUSTMENT',
            'trajectory': msg['requested'],
            'applied': msg['requested'],
            'log': 0
            }
        return json.dumps(reply)
        
logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.DEBUG)            
logging.info(f"Starting up")            
vehicle_to_bridge = VehicleToBridge()
station_to_bridge = []
addrs = psutil.net_if_addrs()
print("Network interfaces are:", addrs.keys())
print("Network interface list:", interfaces())
i = 0
for k in interfaces():
    print(f"Asking about interface {k}")
    station_to_bridge.append(StationToBridge(i, k, 10000+i))
    i = i+1
time.sleep(10000)

