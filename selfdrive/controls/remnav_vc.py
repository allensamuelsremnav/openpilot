#!/usr/bin/env python3

'''
OpenPilot implementation of a Remnav Vehicle Controller

There are three classes. Each class has a global singleton instance and an associated thread.

(1) Is the external message processor, it reads JSON messages from a UDP socket and executes them generated a result JSON
(2) This is the internal timer/timeout thread. It's responsible for any action of the VC that's indpendent of a received message
(3) This is the internal OpenPilot message processor. It listens for OpenPilot messages and extracts any sensor data.

Generally, objects are mutated only by their associated thread and are read-only to other threads. However, there are some exceptions to this.

'''

#
# Configurable Constants
#
# All times are in milliSeconds
# All distances are in meters
#

# UDP Port for communication
VC_PORT_NUMBER = 7777

# Estimated delay from receipt of a message and it's application to the local actuators
APPLIED_TIMESTAMP_DELTA = 10

# Wall time without a receipt of a message before declaring a local communication failure
LAN_TIMEOUT = 1000000 #

import threading, socket, json, time, os
from system.swaglog import cloudlog
import cereal.messaging as messaging

running = True # Thread running

# States of the vc.wan_status variable
WAN_NORMAL = 'normal'
WAN_SHORT_OUTAGE = 'short_outage'
WAN_LONG_OUTAGE = 'long_outage'
WAN_LAN_OUTAGE = 'lan_outage'

# states of the vc.state variable
STATE_SAFETY_DRIVER = 'safety_driver'
STATE_REMOTE_READY = 'remote_ready'
STATE_REMOTE_DRIVER = 'remote_driver'

#
# Centralized logging functions
#
def log_info(msg):
    print(">>>>>> " , msg)
    cloudlog.info(">>Remnav: %s", msg)

def log_critical(msg):
    # Critical messages are always dumped onto the OP screen.
    cloudlog.critical(">>>Remnav: %s", msg)

def timestamp():
    '''Generate internal timestamp'''
    return int(time.time() * 1000)

class GlobalThread:
    '''Base class for all single/threaded objects'''
    def start(self):
        log_info(f"Starting thread {self.runner.__qualname__}")
        self.thread = threading.Thread(target=self.runner, args=())
        self.thread.start()

class VCState(GlobalThread):
    '''The external thread, this also holds the vast majority of state'''
    def __init__(self):
        self.last_message_id = -1
        self.last_message_timestamp = -1
        self.last_received_timestamp = 0
        self.last_applied_timestamp = 0
        self.request_enable = False
        self.acceleration = 0
        self.steering = 0
        self.last_steering = 0
        self.last_acceleration = 0
        self.wan_status = WAN_LONG_OUTAGE
        self.client_address = ('localhost', 0)
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.state = STATE_SAFETY_DRIVER

    def runner(self):
        '''
        Listen to socket, process messages that are recevied on it
        '''
        self.socket.bind(('', VC_PORT_NUMBER))
        log_info(f"Socket bound: to {self.socket.getsockname()}")
        while running:
            #log_info(f"Waiting for input on socket {self.socket.getsockname()}")
            message, address = self.socket.recvfrom(1500)
            #log_info(f"From:{address} : {message}")
            if self.client_address != address:
                log_info(f"New client found at {address}")
                self.client_address = address
            try:
                msg = json.loads(message)
            except json.JSONDecodeError as e:
                log_critical(f"Unparseable JSON received: {message}")
            else:
                if isinstance(msg, dict):
                    try:
                        self.process_message(msg)
                    except ValueError as e:
                        # Typically if you get here, you're missing a field in the JSON message that "process_message" consideres as not optional.
                        log_critical(f"Bad message contents: {message}")
                    else:
                        # Now that the message has been successfully validated, update the local state according.
                        self.update_state()
                        self.send_response(False)
                else:
                    log_critical(f"Bad Message: Not Dictionary: {msg}")
    
    def send_response(self, timeout):
        '''Generate a response message. '''
        js = self.generate_response(timeout)
        s = json.dumps(js).encode()
        self.socket.sendto(s, self.client_address)
    
    def process_message(self, msg):
        '''
        Extract message components and update state
        '''
        if msg['timestamp'] <= self.last_message_timestamp or msg['message_id'] <= self.last_message_id:
            log_info(f"Ignoring apparent late/duplicate message id: {msg['message_id']} timestamp: {msg['timestamp']}")
            return
        self.last_received_timestamp = timestamp()
        self.last_message_timestamp = msg['timestamp']
        self.last_message_id = msg['message_id']
        if 'request_enable' in msg:
            if not isinstance(msg['request_enable'], bool):
                log_info("Ignoring non-boolean request_enable")
                return
            self.request_enable = msg['request_enable']
        if 'acceleration' in msg:
            self.acceleration = msg['acceleration']
        if 'steering' in msg:
            self.steering = msg['steering']
        if 'wan_status' in msg:
            if msg['wan_status'] in (WAN_NORMAL, WAN_SHORT_OUTAGE, WAN_LONG_OUTAGE):
                self.wan_status = msg['wan_status']
            else:
                log_critical(f"Unknown value for wan_status: {msg['wan_status']}")

    def generate_response(self, timeout):
        ts = timestamp()
        return {
            'timestamp': ts,
            'last_message_timestamp': self.last_message_timestamp,
            'last_message_id': self.last_message_id,
            'last_received_timestamp': self.last_received_timestamp,
            'last_applied_timestamp' : ts + APPLIED_TIMESTAMP_DELTA,
            'state': self.state,
            'acceleration': self.acceleration,
            'steering': self.steering,
            'current_steering': self.steering, # since there's no feedback from OP
            'current_speed': op.speed,
            'current_enable': op.enabled,
            'accelerator_override': op.accelerator_override,
            'steering_override': op.steering_override,
            'brake_override': op.brake_override,
            'timeout': timeout,
            'state': self.state,
        }          

    def update_state(self):
        '''
        Compute the new state. This is the business logic
        '''
        if self.state == STATE_REMOTE_READY:
            self.state_remote_ready()
        elif self.state == STATE_SAFETY_DRIVER:
            self.state_safety_driver()
        elif self.state == STATE_REMOTE_DRIVER:
            self.state_remote_driver()
        else:
            assert(False)

    def state_remote_ready(self):
        '''
        Vehicle is ready to enable remote operation
        '''
        if not op.enabled or op.override():
            log_info("Remove Ready -> Safety Driver, No OP Enable")
            self.state = STATE_SAFETY_DRIVER
        elif self.request_enable:
            log_info("Remote Ready -> Remote Driver")
            self.state = STATE_REMOTE_DRIVER

    def state_safety_driver(self):
        '''
        Vehicle in safety_driver state.
        '''
        if op.enabled and not op.override() and self.wan_status == WAN_NORMAL:
            if self.request_enable:
                log_info("SAFETY_DRIVER -> REMOTE_DRIVER")
                self.state = STATE_REMOTE_DRIVER
            else:
                log_info("SAFETY_DRIVER -> REMOTE_READY")
                self.state = STATE_REMOTE_READY
        elif self.request_enable:
            log_info(f"Enable Ignored: OpEnable:{op.enabled} Override:{op.override()} Wan:{self.wan_status}")

    def state_remote_driver(self):
        '''
        Vehicle in remote driving state
        '''
        if op.override() or not op.enabled or self.wan_status != WAN_NORMAL:
            log_info("Safety Driver Override -> Safety Driver")
            self.state = STATE_SAFETY_DRIVER
        elif not self.request_enable:
            log_info("Remote Driver -> Remote Ready")
            self.state = STATE_REMOTE_READY
        else:
            if self.last_steering != self.steering or self.last_acceleration != self.acceleration:
                log_info(f"Received: Steering: {self.last_steering}->{self.steering} Acceleration: {self.last_acceleration}->{self.acceleration}")
            self.last_steering = self.steering
            self.last_acceleration = self.acceleration

class TimerState(GlobalThread):
    def __init__(self):
        self.last_timeout = timestamp()
        self.first_timeout = timestamp()
        pass

    def runner(self):
        log_info("In TimerState::runner")
        while running:
            time.sleep(1.0)
            if (timestamp() - vc.last_received_timestamp) > LAN_TIMEOUT and vc.wan_status != LAN_TIMEOUT:
                log_critical("LAN TIMEOUT DETECTED")
                vc.wan_status = WAN_LAN_OUTAGE
                vc.state = STATE_SAFETY_DRIVER
                self.last_timeout = timestamp()
                self.first_timeout = self.last_timeout
            if (timestamp() - self.last_timeout) > 5000 and vc.wan_status == LAN_TIMEOUT:
                log_info(f"Continued LAN Outage for {(timestamp() - self.first_timeout)//1000} seconds")
                self.last_timeout = timestamp()

class OPState(GlobalThread):
    def __init__(self):
        self.enabled = False
        self.speed = 0
        self.steering = 0
        self.accelerator_override = False
        self.steering_override = False
        self.brake_override = False
        self.last_enabled = False
        self.last_status = timestamp()

    def override(self):
        return self.accelerator_override or self.steering_override or self.brake_override
    
    def runner(self):
        sm = messaging.SubMaster(['carState', 'carControl'])
        log_info("OpState: runner")
        while running:
            sm.update()
            self.speed = sm['carState'].vEgo
            self.steering = sm['carState'].steeringAngleDeg
            self.enabled = sm['carControl'].enabled
            if self.enabled != self.last_enabled:
                log_info(f"OP.Enabled {self.last_enabled}->{self.enabled}")
                self.last_enabled = self.enabled
            if (timestamp() - self.last_status) > 5000:
                self.last_status = timestamp()
                gas = "GasPressed" if self.accelerator_override else ""
                brk = "BrakePressed" if self.brake_override else ""
                log_info(f"STATUS: State:{vc.state} WAN:{vc.wan_status} OP_Enabled:{self.enabled} Request:{vc.request_enabled} Speed:{self.speed} Steering:{self.steering} {gas} {brk}")


# Wheelbase for Vehicle
WHEELBASE = 2.78892 # 2018 Highlander

import math

def curvature_to_steering(curvature):
    '''Convert incoming curvature value to an OpenPilot Steering Value'''
    radius = 1.0 / curvature
    angle = math.asin(WHEELBASE / radius)
    return math.copysign(angle, curvature) * 4.0

class RemnavHijacker:
    def __init__(self):
        log_info("Successfully initialized RemnavHijacker")
        vc.start()
        timer.start()   
        op.start()
   
    def hijack(self, accel, steer, curvature, CS):
        '''Openpilot calls this function on every iteration of the PID controller when enabled. '''
        # Some car events show up in CS, not in messages...
        op.accelerator_override = CS.gasPressed
        op.brake_override = CS.brakePressed
        op.steering_override = CS.steeringPressed

        if vc.state != STATE_REMOTE_DRIVER:
            # Reflect back the inputs, i.e., no change
            return (accel, steer, curvature)
        else:
            # override the controls
            return (vc.acceleration, vc.steering, curvature)

def do_unit_tests():

    pass

if __name__ == "main":
    # Invoked from command line, this is the unit test case.
    do_unit_tests()
else:
    # Loaded, meaning this is production usage in OpenPilot
    vc = VCState()
    timer = TimerState()
    op = OPState()

