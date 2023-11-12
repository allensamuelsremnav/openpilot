import cereal.messaging as messaging
import time
import socket
import json
UDP_IP = "192.168.43.115"
UDP_PORT = 9123

# in subscriber
#sm = messaging.SubMaster(['carState', 'carControl'])
sm = messaging.SubMaster(['carControl'])
udpsocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP 
curTS = round(time.time()*1000)
data = {
"class": "VEHICLE_METADATA",
"timestamp":  curTS, #epoch-time-ms,
"ch": 0,   #carrier, 0-2
"openpilotEngaged": False   #on/off flag
}

while 1:
  sm.update()
  data["openpilotEngaged"]=sm['carControl'].enabled
  data["timestamp"]=round(time.time()*1000)
  udpsocket.sendto(json.dumps(data).encode(), (UDP_IP,UDP_PORT))
  print("time: ", data["timestamp"], "enabled: ", sm['carControl'].enabled, ", latActive: ", sm['carControl'].latActive, ", longActive: ",  sm['carControl'].longActive)

