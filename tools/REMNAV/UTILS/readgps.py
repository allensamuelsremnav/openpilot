import cereal.messaging as messaging
import time
# in subscriber
sm = messaging.SubMaster(['gpsLocationExternal'])
while 1:
  sm.update()
  print("time: ", round(time.time()*1000), "\nGPS: ", sm['gpsLocationExternal'])

