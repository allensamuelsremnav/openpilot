#!/usr/bin/env python
import threading
import socket
import math
import time
import json
from pedal_mapper import pedal_mapper

PORT = 6381

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
    self.pedal_mapper = pedal_mapper()
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
          self.gas = 0
        else:
          self.accel = self.gas
        print(f">> Pedal Gas {self.gas:.02f} Brake:{self.brake:.02f}")
      except ValueError:
        result += b'Syntax error:' + sline[1].encode('utf-8') + ' ' + sline[2].encode('utf-8')
    elif sline[0] == 'j':
      try:
        j = json.loads(sline[' '.join(sline[1:])])
        self.handle_920_json(j)
      except json.JSONDecodeError:
        result += b'JSON decode error'
    elif sline[0] == 'q':
      raise OSError()
    else:
      result += b'Help Message:\r\n' + \
                b'p <brake> <gas> : set brake and gas pedals\n' + \
                b'j <json>\n'                                   + \
                b'H                   : toggle hijack mode\r\n' + \
                b'q                   : quit / close this socket'
    if len(result) != 0:
      result += b'\r\n'
    return result
  def handle_920_json(self, j):
    '''Decoded JSON'''
    #
    # Convert Greg's format to the row format used by Gopal's code
    #
    row={}
    param_array = j["parameters"]
    for param in param_array:
      row[param['name']] = float(param['value'])
    self.accel = self.mapper.calc_from_row(row)

  #
  # Called by controls thread to re-write the lateral plan message
  #
  def modify(self, accel, v_ego, unit_test = False):
    self.v_ego = v_ego
    if not self.isConnected() and not unit_test:
      return accel
    self.counter = self.counter + 1
    if (self.counter % 100) == 0:
      print(f"Current Accel: {self.accel:.02f}")
    return self.accel

if __name__ == '__main__':
  x = Hijacker(True)


