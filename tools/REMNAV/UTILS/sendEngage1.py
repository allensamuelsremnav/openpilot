import cereal.messaging as messaging
import time
import socket
import json
UDP_IP = "192.168.43.115"
UDP_PORT = 9123

# in subscriber
#sm = messaging.SubMaster(['carState', 'carControl'])
sm = messaging.SubMaster(['carControl', 'controlsState'])
udpsocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP 
curTS = round(time.time()*1000)
data = {
"class": "VEHICLE_METADATA",
"timestamp":  curTS, #epoch-time-ms,
"ch": 0,   #carrier, 0-2
"openpilotEnabled": False   #on/off flag
}

while 1:
  sm.update()
  lateralControlActive=False
  lateralControlWhich=sm['controlsState'].lateralControlState.which()
  if lateralControlWhich == "pidState":
        lateralControlActive = sm['controlsState'].lateralControlState.pidState.active
  elif lateralControlWhich == "torqueState":
        lateralControlActive = sm['controlsState'].lateralControlState.torqueState.active

#  if sm['controlsState'].state=='enabled' and sm['controlsState'].enabled and sm['controlsState'].active and  sm['carControl'].latActive:
  if sm['controlsState'].state=='enabled' and sm['controlsState'].enabled and sm['controlsState'].active and lateralControlActive:
     data["openpilotEnabled"]=True
  else:
     data["openpilotEnabled"]=False
  data["timestamp"]=round(time.time()*1000)
  udpsocket.sendto(json.dumps(data).encode(), (UDP_IP,UDP_PORT))
#  print("time: ", data["timestamp"], "flag,latActive,state,enabled,active: ", data["openpilotEnabled"], sm['carControl'].latActive, sm['controlsState'].state, sm['controlsState'].enabled. sm['controlsState'].active)
