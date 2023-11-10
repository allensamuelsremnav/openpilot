import cereal.messaging as messaging
import time
# in subscriber
#sm = messaging.SubMaster(['carControl'])
sm = messaging.SubMaster(['carState', 'carControl'])
while 1:
  sm.update()
  print("time: ", round(time.time()*1000), "enabled: ", sm['carControl'].enabled, ", latActive: ", sm['carControl'].latActive, ", longActive: ",  sm['carControl'].longActive)
  print("time: ", round(time.time()*1000), "speed (m/s): ", str(round(sm['carState'].vEgo, 1)), ", brake: ", str(sm['carState'].brake), ", steeringAngleDeg: ",  str(sm['carState'].steeringAngleDeg), ", actuators.steeringAngleDeg: ", str(sm['carControl'].actuators.steeringAngleDeg))
