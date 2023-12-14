#!/usr/bin/env python
import threading
import socket
import math
import time
import json
from selfdrive.controls.pedal_mapper import PedalMapper
from common.conversions import Conversions as CV

PORT = 6381

class RMState:
  ACTIVE = 1
  SHORT_OUTAGE = 2
  LONG_OUTAGE = 3
  HIJACK_STATE = ["safety_driver", "fully_remote", "short_outage", "long_outage"]
  # Time constants in Seconds
  SHORT_OUTAGE_FRAME_THRESHOLD = 500
  SHORT_OUTAGE_MSG_THRESHOLD = 125
  LONG_OUTAGE_FRAME_THRESHOLD = 1000
  LONG_OUTAGE_MSG_THRESHOLD = 200
  REENGAGE_FRAME_THRESHOLD = 400
  REENGAGE_MSG_THRESHOLD = 75
  LONG_OUTAGE_BRAKE_ACC = -0.75

  def __init__(self, name):
    self.name = name
    self.state = RMState.LONG_OUTAGE
    self.last_frame_metadata = json.loads("{}")
    self.last_frame_timestamp = 0
    self.counter = 0
    self.last_msg_TS = 0
    self.last_frame_TS = 0
    self.last_frame_metadata = None
    self.frame_count = 0

  def handle_frame_metadata(self, jblob):
    self.last_msg_TS = int(time.time() * 1000)
    self.last_frame_metadata = jblob
    self.last_frame_TS = self.last_frame_metadata['frametimestamp']
    self.frame_count = self.frame_count + 1
    if 0 == (self.frame_count % 100):
       print(f"Got frame metadata message msg_TS:{self.last_msg_TS} FrameTS:{self.last_frame_TS}")

  def handle_920_json(self, jblob):
    self.last_g920 = jblob
    if "ButtonEvents" in jblob:
      if "RP" in jblob['ButtonEvents']:
        self.set_state(RMState.ACTIVE)
    else:
      print(f"No ButtonEvents in jblob: {jblob}")

  def update_state(self):
    '''Called at arbitrary times to update the Hijack state'''
    current_TS = int(time.time() * 1000)
    time_since_last_msg = current_TS - self.last_msg_TS
    time_since_last_frame = current_TS - self.last_frame_TS
    if time_since_last_frame > RMState.LONG_OUTAGE_FRAME_THRESHOLD or time_since_last_msg > RMState.LONG_OUTAGE_MSG_THRESHOLD:
      print(f"LONG_OUTAGE: {time_since_last_frame} > {RMState.LONG_OUTAGE_FRAME_THRESHOLD} or {time_since_last_msg} > {RMState.LONG_OUTAGE_MSG_THRESHOLD}")
      self.set_state(RMState.LONG_OUTAGE)
    elif self.state == RMState.ACTIVE:
      if time_since_last_frame > RMState.SHORT_OUTAGE_FRAME_THRESHOLD or time_since_last_msg > RMState.SHORT_OUTAGE_MSG_THRESHOLD:
        print(f"Active->Short outage: {time_since_last_frame} > {RMState.SHORT_OUTAGE_FRAME_THRESHOLD} or {time_since_last_msg} > {RMState.SHORT_OUTAGE_MSG_THRESHOLD}")
        self.set_state(RMState.SHORT_OUTAGE)
    elif self.state == RMState.SHORT_OUTAGE:
      if time_since_last_frame <= RMState.REENGAGE_FRAME_THRESHOLD and time_since_last_msg <= RMState.REENGAGE_MSG_THRESHOLD:
        print(f"Short->Active {time_since_last_frame} <= {RMState.REENGAGE_FRAME_THRESHOLD} and {time_since_last_msg} <= {RMState.REENGAGE_MSG_THRESHOLD}")
        self.set_state(RMState.ACTIVE)
    self.counter = self.counter+1
    if 0 == (self.counter % 200):
      print(f"{self.name} in State:{RMState.HIJACK_STATE[self.state]} last_msg:{time_since_last_msg} last_frame:{time_since_last_frame}")

  def set_state(self, new_state):
    if self.state != new_state:
      print(f">> {self.name}@{time.time():.03} {RMState.HIJACK_STATE[self.state]} => {RMState.HIJACK_STATE[new_state]}")
    self.state = new_state

  def is_engaged(self):
    return self.state != RMState.LONG_OUTAGE

  def getState(self):
    return RMState.HIJACK_STATE[self.state]

class Hijacker:
  def __init__(self, unit_test = False):
    self.threads = []
    self.brake = 0.0
    self.gas = 0.0
    self.displayTime = 10.0 # seconds between lateral plan messages
    self.nextDisplayTime = time.time() + self.displayTime
    self.hijackMode = True
    self.accel = 0
    self.counter = 0
    self.pedal_count = 0
    self.mapper = PedalMapper()
    self.parameters = None
    self.last_frame = 0
    self.last_message = 0
    self.last_message_time = time.time()
    self.rmstate = RMState("Accel")
    if unit_test:
      self.connected = True
    else:
      self.serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      self.serversocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
      self.serversocket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)    
      self.serversocket.bind(("0.0.0.0", PORT))
      self.serversocket.listen(1)
      self.thread = threading.Thread(target = self.listener_thread)
      self.thread.start()
      self.connected = False

  def isConnected(self):
    return any([t.is_alive() for t in self.threads])
    
  def listener_thread(self):
    while True:
      print("Waiting for socket connection for PEDAL ")
      (clientSocket, address) = self.serversocket.accept()
      print(">>>>>> PEDAL Got connection from ", address)
      t = threading.Thread(target = self.socket_handler_thread, args=(clientSocket,))
      t.start()
      self.threads.append(t)
  
  def socket_handler_thread(self, clientSocket):
    clientSocket.send(b"Hello Remnav Pedal Hijacker\r\n")
    line = b''
    try:
      while True:
        chunk = clientSocket.recv(1024)
        if chunk == b'':
          print("Socket error")
          return
        for cc in chunk:
          c = chr(cc).encode()
          if c == b'\r' or c == b'\n':
            clientSocket.send(b'Got Cmd:' + line + b'\r\n')
            r = self.process_line(line)
            if r is not None:
              clientSocket.send(r)
            line = b''
          else:
            line += c
    except OSError:
      print("Got socket error")

  #
  # Actually process the command line, updating whatever variables we have
  # returns any response or error text
  def process_line(self, line):
    result = b""
    if len(line) == 0:
      return
    if chr(line[0]) == '<':
      tag,line = line[1:].split(b'>')
      result += b'<' + tag + b'>'
    sline = line.split(b' ')
    if sline[0] == b'p':
      try:
        self.brake, self.gas = (float(sline[1]), float(sline[2]))
        if self.brake != 0:
          self.accel = -self.brake
          # self.gas = 0
        else:
          self.accel = self.gas
        self.pedal_count = self.pedal_count + 1
        if 0 == (self.pedal_count % 100):
          print(f">> Pedal Gas {self.gas:.02f} Brake:{self.brake:.02f}")
      except ValueError:
        result += b'Syntax error:' + sline[1].encode('utf-8') + ' ' + sline[2].encode('utf-8')
    elif sline[0] == b'j':
      result += self.handle_920_json(b' '.join(sline[1:]))
    elif sline[0] == b'm':
      result += self.handle_parameters(b' '.join(sline[1:]))
    elif sline[0] == b'f':
      result += self.handle_frame_metadata(b' '.join(sline[1:]))
    elif sline[0] == b'q':
      raise OSError()
    else:
      result += b'Help Message:\r\n' + \
                b'p <brake> <gas>     : set brake and gas pedals\r\n' + \
                b'm <json>            : load parameters\r\n' + \
                b'j <json>            : 920 message\r\n' + \
                b'H                   : toggle hijack mode\r\n' + \
                b'q                   : quit / close this socket'
    if len(result) != 0:
      result += b'\r\n'
    return result
  def handle_parameters(self, blob):
    try:
      b = json.loads(blob.decode('utf-8'))
      self.parameters = b["parameters"]
    except:
      print(f"Bad parameters message: {blob}")
    return b''

  def handle_920_json(self, blob):
    '''Decoded JSON'''
    try:
      b = json.loads(blob.decode('utf-8'))
      self.rmstate.handle_920_json(b)
    except json.JSONDecodeError:
      print(f"Bad G920 JSON: {blob}")
      return b'JSON decode error'
    return b''

  def handle_frame_metadata(self, blob):
    try:
      self.frame_metadata = json.loads(blob.decode('utf-8'))
      self.rmstate.handle_frame_metadata(self.frame_metadata)
    except json.JSONDecodeError:
      print(f"Bad Frame Metadata: {blob}")
      return b'JSON decode error'
    return b''

  #
  # Called by controls thread to re-write the lateral plan message
  #
  def modify(self, accel, v_ego, unit_test = False):
    self.v_ego = v_ego
    if not self.isConnected() and not unit_test:
      return accel
    self.counter = self.counter + 1
    if (self.counter % 100) == 0:
      print(f"Current Accel: {self.accel:.02f} State:{RMState.HIJACK_STATE[self.rmstate.state]}")
    #
    # Convert to format used by pedal mapper
    #
    row={}
    if self.parameters is None:
      return self.accel
    for p in self.parameters:
      row[p['name']] = p['value']

    row['current_speed'] = v_ego * CV.MS_TO_MPH
    row['x_throttle'] = self.gas
    row['x_brake'] = self.brake
    self.accel = self.mapper.calc_from_row(row) * 4.0
    return self.accel

