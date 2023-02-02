#!/usr/bin/env python
import threading
import socket
import math
import time

CONTROL_N = 17  # from controls/lib/drive_helpers.py
PORT = 6379
M_PI = math.pi
M_PI_2 = math.pi / 2.0
M_PI_4 = math.pi / 4.0
M_SQRT2 = math.sqrt(2.0)
M_SQRT2_2 = (M_SQRT2 / 2.0)

# copied from selfdrive/models/constant.py
IDX_N = 33
def index_function(idx, max_val=192, max_idx=32):
  return (max_val) * ((idx/max_idx)**2)
T_IDXS = [index_function(idx, max_val=10.0) for idx in range(IDX_N)]

class Vec2:
  def __init__(self, a,b):
    self.x = a
    self.y = b

  def __str__(self):
    return f"[{self.x},{self.y}]"    

def VecAdd(a,b):
  return Vec2(a.x+b.x, a.y+b.y)

def VecSubtract(a,b):
  return Vec2(a.x-b.x, a.y-b.y)

def VecMultiply(a,b):
  if type(b) is Vec2:
    return Vec2(a.x*b.x, a.y*b.y)
  else:
    return Vec2(a.x * b, a.y * b)

def VecNegate(a):
  return Vec2(-a.x, -a.y)

def divide(dividend, divisor):
  try:
    return dividend/divisor
  except ZeroDivisionError as exc:
    if dividend == 0:
      raise ZeroDivisionError from exc
    # instead of raising an error, an alternative
    # is to return float('nan') as the result of 0/0

    if dividend > 0:
      return float('inf')
    else:
      return float('-inf')  

class UnitVector(Vec2):
  def __init__(self, angle):
    super().__init__(math.cos(angle), math.sin(angle))
  def cos(self):
    return self.x
  def sin(self):
    return self.y

class RPY:
  def __init__(self, r, p, y):
    self.roll = r
    self.pitch = p
    self.yaw = y    

def rotatePointAroundPoint(center, point, angle):
  '''Center, Point are vec2, angle is a unitvector '''
  p = VecSubtract(point, center) # // Move to center
  rotated = Vec2((p.x * angle.cos()) - (p.y * angle.sin()), (p.x * angle.sin()) + (p.y * angle.cos()))
  return VecAdd(rotated, center)

#//
#// Move a distance from a point using a heading
#//
def moveHeading(start, distance, heading):
  ''' start is Vec2, distance, headign is unit vector'''
  return VecAdd(start, VecMultiply(heading, distance))

class BicycleModel:
  def __init__(self, *p):
    if len(p) == 1:
      self.m_wheel_base = p[0]
      self.setSteeringAngle(0)
    elif len(p) == 2:
      self.m_wheel_base = p[1]
      self.setSteeringAngle(p[0])
    else:
      assert(False)

  def isStraight(self):
    return math.isinf(self.m_turning_radius)
  def getTurningCenter(self):
    return Vec2(-self.m_wheel_base, self.m_turning_center_y)
  def getTurningRadius(self):
    return self.m_turning_radius
  def getWheelBase(self):
    return self.m_wheel_base
  def getPosition(self):
    return self.m_position
  #//
  #// Get position of a theoretical wheel, negative distances are for "left" side.
  #//
  def getWheelPosition(self, distance):
    return moveHeading(self.m_position, distance, UnitVector(self.m_rotation_angle + M_PI_2))
  def getRotationAngle(self):
    return self.m_rotation_angle
  def getRotationVector(self):
    return self.m_rotation_vector
  def getAccelerationAngle(self):
    return 0.0 if self.isStraight() else self.m_rotation_angle + (-M_PI_2 if self.m_steer_angle < 0 else M_PI_2)
  def getAccelerationVector(self):
    return UnitVector(self.getAccelerationAngle())
  def getSteerAngle(self):
    return self.m_steer_angle


  def setSteeringAngle(self, steer_angle):
    self.m_steer_angle = steer_angle
    self.m_turning_radius =  divide(self.m_wheel_base, abs(math.sin(self.m_steer_angle)))
    if math.isinf(self.m_turning_radius):
      # going straight
      self.m_turning_center_y = 0
    else:
      # Now we have a triangle, compute the third side
      self.m_turning_center_y = math.sqrt(self.m_turning_radius * self.m_turning_radius - self.m_wheel_base * self.m_wheel_base)
    
    if self.m_steer_angle < 0:
      self.m_turning_center_y = -self.m_turning_center_y
    self.setDistance(0.0)
  
  #//
  #// Set the distance travelled from 0. Maybe repeatedly called.
  #//
  def setDistance(self, distance):
    if math.isinf(self.m_turning_radius):
      self.m_rotation_angle = 0.0
      self.m_rotation_vector = UnitVector(0)
      self.m_position = Vec2(distance, 0.0)
    else:
      #// First compute rotation angle
      circumference = 2.0 * math.pi * self.m_turning_radius
      fraction_of_circumference = (distance / circumference) % 1.0
      self.m_rotation_angle = 2.0 * math.pi * fraction_of_circumference
      if self.m_steer_angle < 0.0:
        self.m_rotation_angle = -self.m_rotation_angle
      self.m_rotation_vector = UnitVector(self.m_rotation_angle)
      self.m_position = rotatePointAroundPoint(self.getTurningCenter(), Vec2(0, 0), self.m_rotation_vector)

class T_variables:
  def __init__(self, _t, _v_0, _a, bike):
    self.t = _t
    self.v_0 = _v_0
    self.a = _a
    self.v_t = self.v_0 + (self.a * self.t)     # // Velocity at time t.
    self.v_avg = (self.v_t + self.v_0) / 2.0   # // Average velocity
    self.d = self.v_avg * self.t               # // Distance travelled

    bike.setDistance(self.d)

    self.position = bike.getPosition()
    self.velocity = VecMultiply(bike.getRotationVector(), self.v_avg)
    if math.isinf(bike.getTurningRadius()) or self.t == 0.0:
      self.acceleration = VecMultiply(UnitVector(0.0),self.a)
      self.orientation = RPY(0.0, 0.0, 0.0)
      self.orientationRate = RPY(0.0, 0.0, 0.0)
    else:
      #// We just account for centripetal acceleration and ignore the linear acceleration component
      #// Centripal = V**2 / r
      acceleration_magnitude = (self.v_avg * self.v_avg) / bike.getTurningRadius()
      self.acceleration = VecMultiply(bike.getAccelerationVector(), acceleration_magnitude)
      self.orientation = RPY(0.0, 0.0, bike.getRotationAngle())
      #//
      #// Compute the orientationRate (angular velocity). this is in Radians/second.
      #// orientationRate = 2 * PI * f
      #// where f is in units of circles / second => velocity / circumference
      #// W = 2 * PI * (V / (2 * PI * R)) => V / R
      #//
      w = self.v_avg / bike.getTurningRadius()
      self.orientationRate = RPY(0.0, 0.0, (-w) if bike.getSteerAngle() < 0 else w)

class Hijacker:
  def __init__(self, unit_test = False, wb = 2.78892):
    self.clientSocket = None
    self.steer = 0.0
    self.steerRate = 0.0
    self.prevSteer = 0.0
    self.prevCurvature = 0.0
    self.wheelBase = wb  # Highlander wheel base is 109.8 inches => 2.78892 meters.
    self.steerLimit = 0.244346   # Maximum steering deflection
    self.v_ego = 0.0
    self.displayTime = 10.0 # seconds between lateral plan messages
    self.nextDisplayTime = time.time() + self.displayTime
    self.hijackMode = True
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
    
  def listener_thread(self):
    while True:
      self.clientSoocket = None
      try:
        print("Waiting for socket connection")
        (self.clientSocket, address) = self.serversocket.accept()
        print("Got connection from ", address)
        self.clientSocket.send(b"Hello Remnav\r\n")
        line = b''
        self.connected = True
        while True:
          chunk = self.clientSocket.recv(1024)
          if chunk == b'':
            print("Socket error")
            return
          for cc in chunk:
            c = chr(cc).encode()
            if c == b'\r' or c == b'\n':
              self.clientSocket.send(b'Got Cmd:' + line + b'\r\n')
              self.clientSocket.send(self.process_line(line))
              line = b''
            else:
              line += c
      except OSError:
        print("Got socket error")
        self.clientSocket = None
        self.connected = False

  def setSteer(self, s):
    if s < -self.steerLimit:
      self.steer = -self.steerLimit
    elif s > self.steerLimit:
      self.steer = self.steerLimit
    else:
      self.steer = s
    #
    # Now, generate a current status
    #
    lp = fake_lp()
    self.convert_message(lp, self.v_ego)
    return f'V:{self.v_ego:.1f} Circle:{self.bike.getTurningRadius():.1f} P:{lp.psis[0]:.4f}'.encode('utf-8') + \
      f' {lp.psis[1]:.4f} {lp.psis[2]:.4f}  Curve:{lp.curvatures[0]:.4f} CurveRate:{lp.curvatureRates[0]:.4}'.encode('utf-8')


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
    if sline[0] == b's':
      try:
        result += self.setSteer(-float(sline[1]))
      except ValueError:
        result += b'Syntax error:' + sline[1]
    elif sline[0] == b'c':
      try:
        radius = -float(sline[1])
        if abs(radius) <= self.wheelBase:
          result += b'too small radius'
        else:
          result += self.setSteer(math.asin(self.wheelBase / radius))
      except ValueError:
        result += b'Syntax error:' + sline[1]
    elif sline[0] == b'H':
      self.hijackMode = not self.hijackMode
      result += b'HijackMode is ' + (b'on' if self.hijackMode else b'off')
    elif sline[0] == b'r':
      try:
        self.displayTime = float(sline[1])
        self.nextDisplayTime = time.time() + self.displayTime
      except ValueError:
        result += b'Syntax error:' + sline[1]
    else:
      result += b'Help Message:\r\n' + \
                b's <steer_angle>     : set raw steering angle\r\n' + \
                b'c <circle radius>   : set constant radius circle \r\n' + \
                b'H                   : toggle hijack mode\r\n' + \
                b'r  <seconds>        : set lateral plan display rate'
    if len(result) != 0:
      result += b'\r\n'
    return result

  #
  # Called by controls thread to re-write the lateral plan message
  #
  def convert_message(self, lp, v_ego):
    self.v_ego = v_ego
    if not self.connected:
      return
    
    if self.hijackMode:
      #
      # Now, compute my own psi, curvature, curvatureRates
      #
      self.bike = BicycleModel(self.steer, self.wheelBase) # Initial
      
      for i in range(0, CONTROL_N):
        t = T_IDXS[i]     # time
        tv = T_variables(t, v_ego, 0, self.bike)
        lp.psis[i] = tv.orientation.yaw
        lp.curvatures[i] = 1. / self.bike.getTurningRadius()
        lp.dPathPoints[i] = tv.position.y
      if lp.psis[1] < 0:
        lp.curvatures = [-l for l in lp.curvatures]
      if v_ego == 0.0:
        lp.curvatureRates[0] = 0.0
      else:
        lp.curvatureRates[0] = (lp.curvatures[0] - self.prevCurvature) / v_ego
      self.prev_steer = self.steer
      self.prevCurvature = lp.curvatures[0]
    if self.nextDisplayTime < time.time():
      self.nextDisplayTime = time.time() + self.displayTime
      self.displayMessage(lp, v_ego)

  def displayMessage(self, lp, v_ego):
    print(f"At time: {time.time():.1f}, v_ego:{v_ego:.3f}" +
        " Curve:" + ",".join([f'{lp.curvatures[i]:.3f}' for i in range(4)]) +
        " CrvRate:" + ",".join([f'{lp.curvatureRates[i]:.3f}' for i in range(4)]))
    print("Psis :",",".join([f'{lp.psis[i]:.3f}' for i in range(10)]))
    print("Dpath:",",".join([f'{lp.dPathPoints[i]:.3f}' for i in range(10)]))



def EXPECT_EQ(a,b):
  if type(a) is RPY and type(b) is RPY:
    EXPECT_EQ(a.yaw, b.yaw)
  elif type(a) is Vec2 and type(b) is Vec2:
    EXPECT_EQ(a.x, b.x)
    EXPECT_EQ(a.y, b.y)
  else:
    if not math.isclose(a,b, abs_tol = .001):
      print("NOT EQUAL ", a, b)
      assert(False)

def EXPECT_TRUE(a):
  assert(a)

def EXPECT_DOUBLE_EQ(a,b):
  EXPECT_EQ(a,b)  

def test_moveHeader_zero():
  x = Vec2(1,1)
  for angle in [0.0, math.pi/2.0, math.pi/4.0]:
    EXPECT_EQ(moveHeading(x, 0, UnitVector(angle)), x)
  
  EXPECT_EQ(moveHeading(x, 1.0, UnitVector(0.0)), Vec2(2.0, 1.0))
  EXPECT_EQ(moveHeading(x, 1.0, UnitVector(M_PI/2.0)), Vec2(1.0, 2.0))
  EXPECT_EQ(moveHeading(x, -1.0, UnitVector(M_PI/2.0)), Vec2(1.0, 0.0))
  EXPECT_EQ(moveHeading(Vec2(0, 0), 1.0, UnitVector(M_PI_4)), Vec2(M_SQRT2_2, M_SQRT2_2))

def test_BicycleModel_straight():
  #// Straight
  b = BicycleModel(0, 1)
  EXPECT_EQ(b.getTurningCenter().x, -1.0)
  EXPECT_TRUE(math.isinf(b.getTurningRadius()))
  EXPECT_EQ(b.getPosition(), Vec2( 0.0, 0.0))
  b.setDistance(1)
  EXPECT_EQ(b.getPosition(), Vec2( 1.0, 0.0))
  EXPECT_EQ(b.getWheelPosition( 1.0), Vec2(1.0, 1.0))
  EXPECT_EQ(b.getWheelPosition(-1.0), Vec2(1.0,-1.0))


def test_BicycleModel_Right():
  b = BicycleModel(M_PI_4, 1)  #// 45 degree angle
  EXPECT_EQ(b.getTurningRadius(), M_SQRT2)
  EXPECT_EQ(b.getTurningCenter(), Vec2(-1.0, 1.0))
  EXPECT_EQ(b.getPosition(), Vec2( 0.0, 0.0))
  #// Move PI/4 of the way around the circle.
  circumference = 2 * M_PI * M_SQRT2
  distance = circumference / 8  # // 2*PI radians in the circle
  b.setDistance(distance)
  EXPECT_DOUBLE_EQ(b.getRotationAngle(), M_PI_4)
  EXPECT_EQ(b.getPosition(), Vec2(b.getTurningRadius() - b.getWheelBase(), b.getTurningCenter().y))
  EXPECT_EQ(b.getWheelPosition(1.0) , VecAdd(b.getPosition(), UnitVector(M_PI_4 + M_PI_2)))
  EXPECT_EQ(b.getWheelPosition(-1.0), VecAdd(b.getPosition(), UnitVector(M_PI_4 - M_PI_2)))

def test_BicycleModel_Left():
  b = BicycleModel(-M_PI_4, 1) # // 45 degree angle
  EXPECT_DOUBLE_EQ(b.getTurningRadius(), M_SQRT2)
  EXPECT_EQ(b.getTurningCenter(), Vec2(-1.0, -1.0))
  EXPECT_EQ(b.getPosition(), Vec2( 0.0, 0.0))
  #// Move PI/4 of the way around the circle.
  circumference = 2 * M_PI * M_SQRT2
  distance = circumference / 8 # // 2*PI radians in the circle
  b.setDistance(distance)
  EXPECT_DOUBLE_EQ(b.getRotationAngle(), -M_PI_4)
  EXPECT_EQ(b.getPosition(), Vec2(b.getTurningRadius() - b.getWheelBase(), b.getTurningCenter().y))
  EXPECT_EQ(b.getWheelPosition(1.0) , VecAdd(b.getPosition(), UnitVector(-M_PI_4 + M_PI_2)))
  EXPECT_EQ(b.getWheelPosition(-1.0), VecAdd(b.getPosition(), UnitVector(-M_PI_4 - M_PI_2)))

def test_T_vars_zero():
  bike = BicycleModel(1)
  bike.setSteeringAngle(0.0)
  bike.setDistance(0)
  tv = T_variables(0, 0, 0, bike)
  EXPECT_EQ(tv.position, Vec2(0,0))
  EXPECT_EQ(tv.acceleration, Vec2(0,0))
  EXPECT_EQ(tv.velocity, Vec2(0,0))
  EXPECT_EQ(tv.orientation, RPY(0,0,0))
  EXPECT_EQ(tv.orientationRate, RPY(0,0,0))


def test_T_vars_one():
  b = BicycleModel(1)
  b.setSteeringAngle(0.0)
  tv = T_variables(1.0, 1.0, 0, b)   # // t = 1.0, v = 1.0, a = 0
  EXPECT_EQ(tv.position, Vec2(1.0, 0))
  EXPECT_EQ(tv.acceleration, Vec2(0,0))
  EXPECT_EQ(tv.velocity, Vec2(1.0, 0.0))
  EXPECT_EQ(tv.orientation, RPY(0,0,0))
  EXPECT_EQ(tv.orientationRate, RPY(0,0,0))


def test_T_vars_one_45_right():
  b = BicycleModel(1)
  b.setSteeringAngle(M_PI_4)
  EXPECT_EQ(b.getTurningRadius(), M_SQRT2)
  #//
  #// Compute Time to go PI/8 around the circle
  circumference = 2 * M_PI * b.getTurningRadius()
  distance = circumference / 8 # // 2*PI radians in the circle
  v = distance / 1.0

  tv = T_variables(1.0, v, 0, b)  # // t = 1.0, v = distance/ 1.0 seconds , a = 0
  EXPECT_EQ(tv.position, Vec2(b.getTurningRadius() - b.getWheelBase(), b.getTurningCenter().y))
  # // V**2 / r, r = SQRT(2)
  EXPECT_EQ(tv.velocity, VecMultiply(UnitVector(M_PI_4), v))
  EXPECT_EQ(tv.acceleration, VecMultiply(UnitVector(3 * M_PI_4), (v * v / b.getTurningRadius())))
  EXPECT_EQ(tv.orientation, RPY(0.0, 0.0, M_PI_4))
  EXPECT_EQ(tv.orientationRate, RPY(0.0, 0.0, M_PI_4))


def test_T_vars_two_45_right(): # // 45 to the right, two time units
  b = BicycleModel(1)
  b.setSteeringAngle(M_PI_4)
  EXPECT_EQ(b.getTurningRadius(), M_SQRT2)
  ##//
  #// Compute Time to go PI/8 around the circle
  circumference = 2 * M_PI * b.getTurningRadius()
  distance = circumference / 8 # // 2*PI radians in the circle
  v = distance / 1.0

  tv = T_variables(2.0, v, 0, b) #  // t = 2.0, v = distance/ 1.0 seconds , a = 0
  EXPECT_EQ(tv.position, Vec2(0, 2.0))
  # // V**2 / r, r = SQRT(2)
  EXPECT_EQ(tv.velocity, VecMultiply(UnitVector(M_PI_4 + M_PI_4), v))
  EXPECT_EQ(tv.acceleration, VecMultiply(UnitVector(3 * M_PI_4 + M_PI_4), (v * v / b.getTurningRadius())))
  EXPECT_EQ(tv.orientation, RPY(0.0, 0.0, M_PI_4 + M_PI_4))
  EXPECT_EQ(tv.orientationRate, RPY(0.0, 0.0, M_PI_4))


def test_T_vars_one_45_left(): #// 45 to the right, one unit
  b = BicycleModel(1)
  b.setSteeringAngle(-M_PI_4)
  EXPECT_EQ(b.getTurningRadius(), M_SQRT2)
  #//
  #// Compute Time to go PI/8 around the circle
  circumference = 2 * M_PI * b.getTurningRadius()
  distance = circumference / 8 # // 2*PI radians in the circle
  v = distance / 1.0

  tv = T_variables(1.0, v, 0, b)   # // t = 1.0, v = distance/ 1.0 seconds , a = 0
  EXPECT_EQ(tv.position, Vec2(b.getTurningRadius() - b.getWheelBase(), b.getTurningCenter().y))
  # // V**2 / r, r = SQRT(2)
  EXPECT_EQ(tv.velocity, VecMultiply(UnitVector(-M_PI_4), v))
  EXPECT_EQ(tv.acceleration, VecMultiply(UnitVector(3 * -M_PI_4), (v * v / b.getTurningRadius())))
  EXPECT_EQ(tv.orientation, RPY(0.0, 0.0, -M_PI_4))
  EXPECT_EQ(tv.orientationRate.roll, 0.0)
  EXPECT_EQ(tv.orientationRate.pitch, 0.0)
  assert(math.isclose(tv.orientationRate.yaw, -M_PI_4, abs_tol=.01))

def test_hijack_command():
  h = Hijacker(unit_test = True)
  for (c,s) in [
      (b's .1',         .1), 
      (b's -.15',     -.15), 
      (b'<1>s .1',      .1),
      (b'<2>s .01',    .01)]:
    h.process_line(c)
    EXPECT_EQ(h.steer, -s)

  r = h.process_line(b's .1')
  EXPECT_EQ(h.steer, -.10)
  r = h.process_line(b's -.15')
  EXPECT_EQ(h.steer, .15)
  r = h.process_line(b'<1>s -.1')
  EXPECT_EQ(h.steer, .10)
  r = h.process_line(b'<2.4>s -.15')
  EXPECT_EQ(h.steer, .15)
  del r

class fake_lp:
  def __init__(self):
    self.psis = [0.0] * CONTROL_N
    self.curvatures = [0.0] * CONTROL_N
    self.curvatureRates = [0.0] * CONTROL_N
    self.dPathPoints = [0.0] * CONTROL_N

def test_hijack_zero():
  h = Hijacker(unit_test = True)
  lp = fake_lp()
  h.steer = 0.0
  h.convert_message(lp, 1.0)
  EXPECT_EQ(sum(lp.psis), 0.0)
  EXPECT_EQ(sum(lp.curvatures), 0)
  EXPECT_EQ(sum(lp.curvatureRates), 0)    

def test_hijack_one():
  h = Hijacker(unit_test = True, wb = 1)
  lp = fake_lp()
  h.steer = M_PI/4
  v_ego = 1
  h.nextDisplayTime = 0
  h.convert_message(lp, v_ego)
  circumference = 2 * M_PI * h.bike.getTurningRadius()
  for i in range(CONTROL_N):
    distance = v_ego * T_IDXS[i]
    angle = distance * 2 * M_PI / circumference
    EXPECT_EQ(angle, lp.psis[i])
  EXPECT_EQ(sum(lp.curvatureRates), lp.curvatures[0])

if __name__ == '__main__':
  test_hijack_command()
  test_hijack_zero()
  test_hijack_one()
  test_moveHeader_zero()
  test_BicycleModel_straight()
  test_BicycleModel_Left()
  test_BicycleModel_Right()
  test_T_vars_zero()
  test_T_vars_one()  
  test_T_vars_one_45_right()
  test_T_vars_two_45_right()
  test_T_vars_one_45_left()

