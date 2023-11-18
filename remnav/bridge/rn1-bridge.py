import json, platform
import time
import socket
import threading
import logging
import argparse
import sys
# from netifaces import ifaddresses, AF_INET, interfaces
import math
import psutil
import os

#
# Parse arguments
#
parser = argparse.ArgumentParser()
parser.add_argument('-station_ip',help="IP Address of operator station", default="96.64.247.70") # was 96.64.247.70
parser.add_argument('-station_port', help="Port number of operator station", type=int, default=6002) # was 
parser.add_argument('-vehicle_ip', help='IP Address of OpenPilot', default='192.168.43.1') # was .1
parser.add_argument('-acc_port', help='Port number for vehicle ACC controller', type=int, default=6381)
parser.add_argument('-mpc_port', help='Port number for vehicle MPC controller', type=int, default=6379)
parser.add_argument('-interfaces', metavar='InterfaceName', type=str, nargs='+',
                    help='Names of local NIC interfaces', default=['wwan0_1', 'wwan1_1', 'wwan2_1'])
args = parser.parse_args()

station_address = (args.station_ip, args.station_port)

#
# rn1-bridge
#
def make_socket_for_interface(interface, bindto = None):
    if not hasattr(socket,'SO_BINDTODEVICE'):
        socket.SO_BINDTODEVICE = 25
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if platform.system() == 'Linux':
        result = sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, str(interface + '\0').encode('utf-8'))
        logging.info(f"Binding to {interface}, result is {result}")
        # assert result == 0
    if bindto:
        sock.bind(bindto)
    return sock

last_rcv_msg_time = time.time()
last_rcv_timestamp = 0

def make_message_for_index(index, msg):
    return chr(index).encode('utf-8') + msg.encode('utf-8')

def make_beacon_for_index(index):
    return make_message_for_index(index,'{"class":"TRAJECTORY_APPLICATION","trajectory":0,"applied":0,"log":0}\n')

class VehicleToBridge:
    def __init__(self, name, vehicle_address, handle_reply_function):
        self.run = True
        self.name = name
        self.connected = False
        self.vehicle_address = vehicle_address
        self.handle_reply_function = handle_reply_function
        self.start()
    
    def start(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.thread = threading.Thread(target=self.runner)
        self.thread.start()

    def stop(self):
        self.run = False
        self.socket.close()
        self.thread.join()

    def send(self, msg):
        if not self.connected:
           # print(f"{self.name} not connected skipping {msg}")
           return
        try:
            self.socket.send(msg)
            #logging.info(f"{self.name} Send: {msg}")
        except:
            #logging.info(f"{self.name} send failure, retrying")
            self.stop()
            self.start()

    def runner(self):
        logging.info(f"Waiting to connect {self.name} controller at {self.vehicle_address}")
        while self.run:
            self.connected = False
            try:
                self.socket.connect(self.vehicle_address)
            except ConnectionRefusedError:
                continue
            except socket.timeout:
                continue
            except OSError:
                continue
            logging.info(f"Established connection to {self.name} at address {self.socket.getpeername()}")
            self.connected = True
            while self.run:
                try:
                    buffer = self.socket.recv(1024).decode('utf-8')
                    # print(f"{self.name} received {buffer}")
                    #
                    # Need to accumulate partial messages and parse out only till newlines to pass into handle function
                    #
                    for msg in buffer.split('\n'):
                        if msg.startswith('<'):
                            tag = msg[1:].split('>')[0]
                            reply = self.handle_reply_function(tag)
                except socket.timeout:
                    continue
                except ConnectionAbortedError:
                    break
                except ConnectionResetError:
                    break

def make_MPC_reply(tag):
    reply = {
        'class': 'TRAJECTORY_APPLICATION',
        'trajectory': int(tag),
        'applied': time.time_ns()//1000,
        'log': 0
    }
    #logging.info(f"Received MPC Vehicle Tag: {tag}, Reply:{reply}")
    StationToBridge.broadcast_reply(json.dumps(reply))

def make_ACC_reply(tag):
    logging.info(f"Dropping ACC reply {tag}")
    pass

HEARTBEAT_TIME = 5.0 # seconds
last_trajectory_timestamp = 0
last_g920_timestamp = 0

class StationToBridge:
    '''One per interface'''
    instances = {}
    def __init__(self, index, interface, port):
        self.index = index
        self.interface = interface
        global last_rcv_msg_time, last_rcv_timestamp
        self.port = port
        self.socket = make_socket_for_interface(interface, bindto=('',0))
        self.socket.settimeout(HEARTBEAT_TIME)
        self.thread = threading.Thread(target=self.listener)
        StationToBridge.instances[index] = self
        self.thread.start()

    def broadcast_reply(m):
        #logging.info(f"Sending: {m}")
        for s in StationToBridge.instances.values():
            s.socket.sendto(make_message_for_index(s.index, m), station_address)

    def listener(self):
        global last_trajectory_timestamp
        global last_g920_timestamp 
        logging.info(f"Sending beacon from {self.interface} at {self.socket.getsockname()[1]} to {station_address}")
        b = make_beacon_for_index(self.index)
        #print("Bytes are: ",b)
        bytes = self.socket.sendto(b, station_address)
        # logging.info(f"Sent {bytes} bytes")
        while True:
            try:
                #logging.info(f"Waiting for response on {self.interface}")
                self.socket.settimeout(HEARTBEAT_TIME)
                message, address = self.socket.recvfrom(1024)
                #logging.info(f"Received from {address} msg: {message}")
                msg = json.loads(message)
                request_timestamp = int(msg['requested'])
                if msg['class'] == "TRAJECTORY":
                    if request_timestamp > last_trajectory_timestamp:
                        last_trajectory_timestamp = request_timestamp
                        StationToBridge.handle_trajectory_message(msg)
                    else:
                        #logging.info(f"Discarding message to {self.interface}, duplicate")
                        pass
                elif msg['class'] == "G920":
                    if request_timestamp > last_g920_timestamp:
                        last_g920_timestamp = request_timestamp
                        StationToBridge.handle_g920_message(msg)
                    else:
                        #logging.info(f"Discarding G920 message, duplicate {request_timestamp} <= {last_g920_timestamp}")
                        pass
                else:
                    logging.info(f"Unknown message class {msg['class']}")

            except (socket.timeout, ConnectionResetError):
                logging.info(f"Sending beacon from {self.interface} at {self.socket.getsockname()[1]} to {station_address}")
                self.socket.sendto(make_beacon_for_index(self.index), station_address)

    def handle_trajectory_message(msg):
        ''' Handle receipt of a valid trajectory message. '''
        curvature = msg['curvature']
        if math.isinf(curvature):
            radius = 0
        elif curvature == 0:
            radius = 10000000000000.0
        else:
            radius = 1 / curvature
            #sradius = 7*radius
        #
        # Make message in format for MPC controller
        # 
        mpc_msg = f"<{msg['requested']}>c {-radius}\r\n"
        #print(f"Sending trajectory radius {-radius}")
        vehicle_mpc.send(mpc_msg.encode('utf-8'))

    def handle_g920_message(msg):
        ''' Handle receipt of valid Pedal message '''
        #print(f"G920: {msg}")
        brake = msg['pedalmiddle']
        accel = msg['pedalright']
        if brake != 0:
            setting = brake
        else:
            setting = accel
        acc_msg = f"p {brake*4.0} {accel*4.0}\r\n"
        print(f"Sending ACC message brake:{brake*4.0} accel:{4.0*accel}")
        vehicle_acc.send(acc_msg.encode('utf-8'))
        
logging.basicConfig(format='%(asctime)s.%(msecs)03d %(levelname)s:%(message)s', level=logging.DEBUG, datefmt='%H:%M:%S')
vehicle_acc = VehicleToBridge('ACC', (args.vehicle_ip, args.acc_port), make_ACC_reply)
vehicle_mpc = VehicleToBridge('MPC', (args.vehicle_ip, args.mpc_port), make_MPC_reply)
station_to_bridge = []
i = 0
interfaces = args.interfaces
if platform.system() == 'Windows':
    interfaces = ["127.0.0.1", "127.0.0.2", "127.0.0.3"]
print(f"Running on platform {platform.system()}, connecting to interfaces: {interfaces}")

for k in interfaces:
    station_to_bridge.append(StationToBridge(i, k, 10000+i))
    i = i+1
time.sleep(10000)

