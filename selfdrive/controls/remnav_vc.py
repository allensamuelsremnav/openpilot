#!/usr/bin/env python3

'''
OpenPilot implementation of a Remnav Vehicle Controller

This implementation creates three independent threads.

(1) Is the external message processor, it reads JSON messages from a UDP socket and executes them generated a result JSON
(2) This is the internal timer/timeout thread. It's responsible for any action of the VC that's indpendent on a received message
(3) This is the internal OpenPilot message processor. It listens for OpenPilot messages and extracts any sensor data.

Each thread has associated with it a singleton global object. That object is only mutated by the associated thread,
However, any thread can read any of the global objects.

'''

#
# Configurable Constants
#
VC_PORT_NUMBER = 7777
APPLIED_TIMESTAMP_DELTA = 10 # Estimate of application delay
LAN_TIMEOUT = 1000 # In milliseconds

import threading, socket, json, time, os
from system.swaglog import cloudlog
import cereal.messaging as messaging

if __name__ == "__main__":
        pass

running = True # Thread running

WAN_NORMAL = 'normal'
WAN_SHORT_OUTAGE = 'short_outage'
WAN_LONG_OUTAGE = 'long_outage'
LAN_OUTAGE = 'lan_outage'

STATE_SAFETY_DRIVER = 'safety_driver'
STATE_REMOTE_READY = 'remote_ready'
STATE_REMOTE_DRIVER = 'remote_driver'

def log_info(msg):
    print(">>>>>> %s", msg)
    cloudlog.info(">>Remnav: %s", msg)

def log_critical(msg):
    print("***>>> %s", msg)
    cloudlog.critical(">>>Remnav: %s", msg)


def timestamp():
    return int(time.time() * 1000)

class GlobalThread:
    def start(self):
        self.thread = threading.Thread(target=self.runner, args=())

class VCState(GlobalThread):
    def __init__(self):
        self.last_message_id = -1
        self.last_message_timestamp = -1
        self.last_received_timestamp = 0
        self.last_applied_timestamp = 0
        self.request_enable = False
        self.acceleration = 0
        self.steering = 0
        self.op_steering = 0
        self.op_acceleration = 0
        self.wan_status = WAN_LONG_OUTAGE
        self.current_enable = True # Until we tie into OP local enable...
        self.last_address = ('localhost', 0)
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.state = STATE_SAFETY_DRIVER

    def runner(self):
        '''
        Listen to socket, process messages that are recevied on it
        '''
        self.socket.bind(('', VC_PORT_NUMBER))
        log_info("Socket bound")
        while running:
            message, address = self.socket.recvfrom(1500)
            log_info(f"From:{address} : {message}")
            if self.last_address != address:
                log_info(f"New client found {address}")
                self.last_address = address
            try:
                msg = json.loads(message)
            except json.JSONDecodeError as e:
                log_critical(f"Unparseable JSON received: {message}")
            else:
                try:
                    self.process_message(msg)
                except ValueError as e:
                    log_critical(f"Bad message contents: {message}")
                else:
                    self.send_response(False)
    
    def send_response(self, timeout):
        self.socket.sendto(self.generate_response(timeout), self.last_address)
    
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
        self.update_state()

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
            'current_enable': self.current_enable,
            'accelerator_override': op.accelerator_override,
            'steering_override': op.steering_override,
            'brake_override': op.brake_override,
            'timeout': timeout,
            'state': self.current_state
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
        if self.request_enable:
            log_info("Remote Ready -> Remote Driver")
            self.state = STATE_REMOTE_DRIVER

    def state_safety_driver(self):
        '''
        Vehicle in safety_driver state.
        '''
        if self.current_enable and not op.override() and self.wan_status == WAN_NORMAL:
            log_info("SAFETY_DRIVER -> REMOTE_READY")
            self.state = STATE_REMOTE_READY
        elif self.current_enable:
            log_info("Enable Ignored: Override:%s Wan:%s", (op.override(), self.wan_status))

    def state_remote_driver(self):
        '''
        Vehicle in remote driving state
        '''
        if not self.request_enable:
            log_info("Remote Driver -> Remote Ready")
            self.state = STATE_REMOTE_READY
        elif op.override():
            log_info("Safety Driver Override -> Remote Ready")
            self.state = STATE_REMOTE_READY
        elif self.wan_status == WAN_NORMAL:
            if self.op_steering != self.steering or self.op_acceleration != self.acceleration:
                log_info(f"Received: Steering: {self.op_steering}->{self.steering} Acceleration: {self.op_acceleration}->{self.acceleration}")
            self.op_steering = self.steering
            self.op_acceleration = self.acceleration
        else:
            # WAN Outage of some flavor
            pass

class TimerState(GlobalThread):
    def __init__(self):
        pass

    def runner(self):
        while running:
            time.sleep(1.0)
            if (timestamp() - vc.last_received_timestamp) > LAN_TIMEOUT and vc.wan_status != LAN_TIMEOUT:
                log_critical("LAN TIMEOUT DETECTED")
                vc.wan_status = LAN_TIMEOUT
                vc.state = STATE_SAFETY_DRIVER


class OPState(GlobalThread):
    def __init__(self):
        self.op_enabled = False
        self.speed = 0
        self.steering = 0
        self.accelerator_override = False
        self.steering_override = False
        self.brake_override = False

    def override(self):
        return self.accelerator_override or self.steering_override or self.brake_override
    
    def runner(self):
        sm = messaging.SubMaster(['carState', 'carControl'])
        while running:
            sm.update()
            self.speed = sm['carState'].vEgo
            self.steering = sm['carState'].steeringAngleDeg
            if self.op_enabled != sm['carControl'].enabled:
                log_info(f"OP Enable {self.op_enabled}->{sm['carControl'].enabled}")
            self.op_enabled = sm['carControl'].enabled

class RemnavHijacker:
    def __init__(self):
        log_info("Successfully initialized RemnavHijacker")
        vc.start()
        timer.start()
        op.start()
   
    def hijack(self, accel, steer, curvature):
        if vc.state != STATE_REMOTE_DRIVER:
            return (accel, steer, curvature)
        else:
            return (vc.acceleration, vc.steering, curvature)

vc = VCState()
timer = TimerState()
op = OPState()

