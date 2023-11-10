#!/usr/bin/env python
import os
import argparse
import threading
from inputs import get_gamepad
import socket
import cereal.messaging as messaging
from common.realtime import Ratekeeper
from common.numpy_fast import interp, clip
from common.params import Params
from tools.lib.kbhit import KBHit
UDP_IP = "0.0.0.0"
UDP_PORT = 9123
axes_order = ['ABS_Y',  'ABS_Z']

def send_thread():
  joystick_sock = messaging.pub_sock('testJoystick')
  server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  server_socket.bind((UDP_IP, UDP_PORT))
  while True:
    message, address = server_socket.recvfrom(1024)
    message=json.loads(message.decode())
    dat = messaging.new_message('testJoystick')
    dat.testJoystick.axes = [message[a] for a in axes_order]
    dat.testJoystick.buttons = [message['cancel']]
    joystick_sock.send(dat.to_bytes())
    print("axes_order[0]: ", message[axes_order[0]], "axes_order[1]: ", message[axes_order[1]])

if __name__ == '__main__':
  print("running joystick server")
  send_thread()
