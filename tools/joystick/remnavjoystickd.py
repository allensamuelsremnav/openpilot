#!/usr/bin/env python
import os
import argparse
import threading
import socket

import cereal.messaging as messaging
from common.realtime import Ratekeeper
from common.numpy_fast import clip
from common.params import Params
from tools.lib.kbhit import KBHit


class Keyboard:
  def __init__(self):
    self.kb = KBHit()
    self.axis_increment = 0.05  # 5% of full actuation each key press
    self.axes_map = {'w': 'gb', 's': 'gb',
                     'a': 'steer', 'd': 'steer'}
    self.axes_values = {'gb': 0., 'steer': 0.}
    self.axes_order = ['gb', 'steer']
    self.cancel = False

  def update(self):
    key = self.kb.getch().lower()
    self.cancel = False
    if key == 'r':
      self.axes_values = {ax: 0. for ax in self.axes_values}
    elif key == 'c':
      self.cancel = True
    elif key in self.axes_map:
      axis = self.axes_map[key]
      incr = self.axis_increment if key in ['w', 'a'] else -self.axis_increment
      self.axes_values[axis] = clip(self.axes_values[axis] + incr, -1, 1)
    else:
      return False
    return True


class Remnav:
  def __init__(self, port):
    self.serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    self.serversocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    self.serversocket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)    
    self.serversocket.bind(("0.0.0.0", port))
    self.serversocket.listen(1)
    self.axes_values = {'gb': 0., 'steer': 0.}
    self.axes_order = ['gb', 'steer']
    self.cancel = False


  def update(self):
    self.connected = False
    try:
      print("Waiting for socket connection")
      (clientsocket, address) = self.serversocket.accept()
      print("Got connection from ", address)
      line = b''
      while True:
        chunk = clientsocket.recv(1024)
        if chunk == b'':
          print("Socket error")
          return
        for cc in chunk:
          c = chr(cc).encode()
          if c == b'\r' or c == b'\n':
            self.process_line(line, clientsocket)
            line = b''
          else:
            line += c
    except OSError:
      pass
  def process_line(self, line, clientsocket):
    if len(line) == 0:
      return
    if line[0:1] == b'<':
      tag,line = line[1:].split(b'>')
      clientsocket.send(b'<' + tag + b'>\r\n')
    sline = line.split(b' ')
    if sline[0] == b's':
      # steering [-1,+1]
      self.axes_values['steer'] = float(sline[1])
    elif sline[0] == b'gb':
      self.axes_values['gb'] = float(sline[1])

def send_thread(joystick):
  joystick_sock = messaging.pub_sock('testJoystick')
  rk = Ratekeeper(100, print_delay_threshold=None)
  prev_gb = 0
  prev_steer = 0
  while 1:
    dat = messaging.new_message('testJoystick')
    dat.testJoystick.axes = [joystick.axes_values[a] for a in joystick.axes_order]
    dat.testJoystick.buttons = [joystick.cancel]
    joystick_sock.send(dat.to_bytes())
    if prev_gb != joystick.axes_values['gb'] or prev_steer != joystick.axes_values['steer']:
      print('\n' + ', '.join(f'{name}: {round(v, 3)}' for name, v in joystick.axes_values.items()))
    prev_gb = joystick.axes_values['gb']
    prev_steer = joystick.axes_values['steer']
    if "WEB" in os.environ:
      import requests
      requests.get("http://"+os.environ["WEB"]+":5000/control/%f/%f" % tuple([joystick.axes_values[a] for a in joystick.axes_order][::-1]), timeout=None)
    rk.keep_time()

def joystick_thread(joystick):
  Params().put_bool('JoystickDebugMode', True)
  threading.Thread(target=send_thread, args=(joystick,), daemon=True).start()
  while True:
    joystick.update()

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Publishes events from your joystick to control your car.\n' +
                                               'openpilot must be offroad before starting joysticked.',
                                   formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument('--keyboard', action='store_true', help='Use your keyboard instead of a joystick')
  parser.add_argument('--port', default=6379, type=int, help="port for remnav bridge connection")
  args = parser.parse_args()

  #if not Params().get_bool("IsOffroad") and "ZMQ" not in os.environ and "WEB" not in os.environ:
  #  print("The car must be off before running remnavjoystickd.")
  #  exit()

  print()
  if args.keyboard:
    print('Gas/brake control: `W` and `S` keys')
    print('Steering control: `A` and `D` keys')
    print('Buttons')
    print('- `R`: Resets axes')
    print('- `C`: Cancel cruise control')
  else:
    print('Waiting for Remnav Connection on port ', args.port)

  joystick = Keyboard() if args.keyboard else Remnav(args.port)
  joystick_thread(joystick)
