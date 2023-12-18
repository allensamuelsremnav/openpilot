#!/usr/bin/env python
import threading
import socket
import math
import time
import json
from selfdrive.controls.pedal_mapper import PedalMapper
from common.conversions import Conversions as CV
from cereal import log
OPState = log.ControlsState.OpenpilotState

PORT = 6381

class RMState:
  ACTIVE = 1
  SHORT_OUTAGE = 2
  LONG_OUTAGE = 3
  HIJACK_STATE = ["safety_driver", "fully_remote", "short_outage", "long_outage"]
  # Time constants in Seconds
  SHORT_OUTAGE_FRAME_THRESHOLD = 500
  SHORT_OUTAGE_MSG_THRESHOLD = 500 # 250 # 125
  LONG_OUTAGE_FRAME_THRESHOLD = 1500 # 1000
  LONG_OUTAGE_MSG_THRESHOLD = 1500 # 500 # 200
  REENGAGE_FRAME_THRESHOLD = 400
  REENGAGE_MSG_THRESHOLD = 400 # 75
  LONG_OUTAGE_BRAKE_ACC = -0.75

  def __init__(self, name):
    self.name = name
    self.state = RMState.LONG_OUTAGE
    self.last_frame_metadata = json.loads("{}")
    self.last_frame_timestamp = 0
    self.counter = 0
    self.last_recv_TS = 0
    self.last_msg_TS = 0
    self.last_frame_TS = 0
    self.frame_count = 0
    self.long_outage_count = 0
    self.last_update_TS = int(time.time() * 1000)
    self.previous_msg_TS = 0

  def handle_frame_metadata(self, jblob):
    self.last_rcv_TS = int(time.time() * 1000)
    self.last_frame_metadata = jblob
    self.previous_msg_TS = self.last_msg_TS
    self.last_msg_TS = self.last_frame_metadata['timestamp']
    self.last_frame_TS = self.last_frame_metadata['frametimestamp']
    self.frame_count = self.frame_count + 1
    msg_delay = self.last_rcv_TS - self.last_msg_TS
    msg_time = self.last_msg_TS - self.previous_msg_TS
    if msg_delay > RMState.LONG_OUTAGE_MSG_THRESHOLD or msg_time > 50:
       drop_time = self.last_msg_TS - self.previous_msg_TS
       print(f"{self.name} Received Late: inter_msg_time: {msg_time} this_msg_lag:{msg_delay} Msg:{self.last_msg_TS} > {RMState.LONG_OUTAGE_MSG_THRESHOLD} Frame:{self.last_frame_TS}")
    if 0 == (self.frame_count % 1000):
       print(f"Got frame metadata message msg_TS:{self.last_msg_TS} msg_diff: {self.last_rcv_TS-self.last_msg_TS} Frame_diff:{self.last_rcv_TS-self.last_frame_TS}")


  def handle_920_json(self, jblob):
    self.last_g920 = jblob
    if "ButtonEvents" in jblob:
      if jblob['ButtonEvents'] is not None:
        if "RP" in jblob['ButtonEvents']:
          self.set_state(RMState.ACTIVE)
        else:
          print(f"920: {jblob}")
    else:
      print(f"No ButtonEvents in jblob: {jblob}")

  def update_state(self):
    '''Called at arbitrary times to update the Hijack state'''
    current_TS = int(time.time() * 1000)
    time_since_last_update = current_TS - self.last_update_TS
    time_since_last_msg = current_TS - self.last_msg_TS
    time_since_last_frame = current_TS - self.last_frame_TS
    if time_since_last_frame > RMState.LONG_OUTAGE_FRAME_THRESHOLD or time_since_last_msg > RMState.LONG_OUTAGE_MSG_THRESHOLD:
      self.long_outage_count = self.long_outage_count + 1
      if 0 == (self.long_outage_count % 1) or self.state != RMState.LONG_OUTAGE:
        msg_time = self.last_msg_TS - self.previous_msg_TS
        print(f"{self.name} LONG_OUTAGE: inter_message_delay:{msg_time} last_pid:{time_since_last_update} last_msg: {self.last_msg_TS} {time_since_last_msg} > {RMState.LONG_OUTAGE_MSG_THRESHOLD} frame: {time_since_last_frame} > {RMState.LONG_OUTAGE_FRAME_THRESHOLD}")
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
    # update
    self.last_update_TS = current_TS

  def set_state(self, new_state):
    if self.state != new_state:
      print(f">> {self.name}@{time.time():.03} {RMState.HIJACK_STATE[self.state]} => {RMState.HIJACK_STATE[new_state]}")
    self.state = new_state

  def is_long_outage(self):
    return self.state == RMState.LONG_OUTAGE

  def is_short_outage(self):
    return self.state == RMState.SHORT_OUTAGE

  def is_active(self):
    return self.state == RMState.ACTIVE

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
    self.opstate = "startup"
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
    else:
      tag = None
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
          print(f">> Pedal Gas {self.gas:.02f} Brake:{self.brake:.02f} tag:{tag}")
      except ValueError:
        result += b'Syntax error:' + sline[1].encode('utf-8') + ' ' + sline[2].encode('utf-8')
    elif sline[0] == b'j':
      result += self.handle_920_msg(b' '.join(sline[1:]))
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

  def handle_920_msg(self, blob):
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
    reply = {
      "class" : "OPSTATE",
      "opstate" : self.opstate
    }
    return json.dumps(reply).encode('utf-8')

  #
  # Called by controls thread to re-write the lateral plan message
  #
  def modify(self, accel, v_ego, opstate, unit_test = False):
    self.v_ego = v_ego
    if not self.isConnected() and not unit_test:
      return accel
    self.rmstate.update_state()
    if opstate == OPState.disabled:
      self.opstate = "disabled"
    elif opstate == OPState.enabled:
      self.opstate = "enabled"
    elif opstate == OPState.softDisabling:
      self.opstate = "softDisabling"
    elif opstate == OPState.overriding:
      self.opstate = "overriding"
    elif opstate == OPState.preEnabled:
      self.opstate = "preEnabled"
    else:
      self.opstate = "unknown"
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
    if self.rmstate.is_short_outage():
       the_accel = min(self.accel, accel)
    elif self.rmstate.is_long_outage():
       the_accel = min(accel, RMState.LONG_OUTAGE_BRAKE_ACC)
    else:
       the_accel = self.accel
    return the_accel

