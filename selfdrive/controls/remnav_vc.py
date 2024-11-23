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
# All times are in microseconds
# All distances are in meters
#

# UDP Port for communication
VC_PORT_NUMBER = 7777

# Estimated delay from receipt of a message and it's application to the local actuators
APPLIED_TIMESTAMP_DELTA = 10_000

# Wall time without a receipt of a message before declaring a local communication failure
LAN_TIMEOUT = 10_000_000

import threading, socket, json, time, os
if 'VC_UNIT_TEST' not in os.environ:
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

# valid message ids
BEACON_ID = 0
TRAJECTORY_ID = 1
G920_ID = 2
FRAME_METADATA_ID = 3

#
# Centralized logging functions
#
def log_info(msg):
    if 'VC_UNIT_TEST' not in os.environ:
        cloudlog.info(">>Remnav: %s", msg)

def log_critical(msg):
    # Critical messages are always dumped onto the OP screen.
    print(">>>>>> " , msg)
    if 'VC_UNIT_TEST' not in os.environ:
        cloudlog.critical(">>>Remnav: %s", msg)

def log_file (msg, direction):
    if 'VC_UNIT_TEST' in os.environ:
        if direction == 'tx':
            tx_file_writer.save_message (msg)
        elif direction == 'rx':
            rx_file_writer.save_message (msg)
        else:
            log_critical (f'unrecoginzied log_file direction {direction}')

def timestamp():
    '''Generate internal timestamp'''
    return int(time.time() * 1_000_000)

class GlobalThread:
    '''Base class for all single/threaded objects'''
    def start(self, daemon=None):
        log_info(f"Starting thread {self.runner.__qualname__}")
        self.thread = threading.Thread(target=self.runner, args=(), daemon=daemon)
        self.thread.start()

class VCState(GlobalThread):
    '''The external thread, this also holds the vast majority of state'''
    def __init__(self):
        self.last_message_id = -1
        self.last_message_timestamp = {
            BEACON_ID: -1,
            TRAJECTORY_ID: -1,
            G920_ID: -1,
            FRAME_METADATA_ID: -1,
        }
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
        self.lock = threading.Lock ()

    def runner(self):
        '''
        Listen to socket, process messages that are recevied on it
        '''
        self.socket.bind(('', VC_PORT_NUMBER))
        log_critical(f"Socket bound: to {self.socket.getsockname()}")
        while running:
            #log_info(f"Waiting for input on socket {self.socket.getsockname()}")
            message, address = self.socket.recvfrom(1500)
            #log_info(f"From:{address} : {message}")
            if self.client_address != address:
                log_info(f"New client found at {address}")
                self.client_address = address

            try:
                msg = json.loads(message)
                msg['address'] = address; 
                log_file (msg, direction='rx')
            except json.JSONDecodeError as e:
                log_critical(f"Unparseable JSON received: {message}")
            else:
                if isinstance(msg, dict):
                    try:
                        with self.lock:
                            response_required = self.process_message(msg) # no response for duplicate ooo messages
                    except ValueError as e:
                        # Typically if you get here, you're missing a field in the JSON message that "process_message" consideres as not optional.
                        log_critical(f"Bad message contents: {message}")
                    else:
                        # Now that the message has been successfully validated, update the local state according.
                        if response_required: # no response for duplicate or out-of-order message
                            with self.lock:
                                self.update_state()
                            self.send_response(False)
                else:
                    log_critical(f"Bad Message: Not Dictionary: {msg}")
    
    def send_response(self, timeout):
        '''Generate a response message. '''
        js = self.generate_response(timeout)
        s = json.dumps(js).encode()
        self.socket.sendto(s, self.client_address)
        js['address'] = self.client_address; 
        log_file (js, 'tx')
    
    def process_message(self, msg):
        '''
            Extract message components and update state.
            Returns True if a response needs to be generated for this message.
            Reponses are not required for duplicate and out-of-order messages
        '''
        if not 'message_id' in msg:
            log_critical(f"Missing message id in {msg}. Ignoring this message")
            raise ValueError (f"Missing message id field in {msg}" )

        if not msg['message_id'] in (BEACON_ID, TRAJECTORY_ID, G920_ID, FRAME_METADATA_ID):
            log_critical(f"Invalid message id in {msg}. Ignoring this message")
            raise ValueError (f"Invalid message id field {msg['message_id']}")

        if msg['timestamp'] <= self.last_message_timestamp[msg['message_id']]:
            log_info(f"Ignoring apparent late/duplicate message id: {msg['message_id']} timestamp: {msg['timestamp']}")
            return False
        self.last_message_timestamp[msg['message_id']] = msg['timestamp']

        self.last_received_timestamp = timestamp()

        self.last_message_id = msg['message_id']

        if 'request_enable' in msg:
            if not isinstance(msg['request_enable'], bool):
                log_info("Ignoring non-boolean request_enable")
                raise ValueError (f"Non-boolean request enable {msg['request_enable']}")
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
                raise ValueError (f"Invalid wan_status f{msg['wan_status']}")

        return True
    # end of process_message

    def generate_response(self, timeout):
        ts = timestamp()
        return {
            'timestamp': ts,
            'last_message_timestamp': self.last_message_timestamp[self.last_message_id],
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
        }          
    # end generate_response

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
        if not op.enabled or op.override() or self.wan_status != WAN_NORMAL:
            log_info("Remove Ready -> Safety Driver")
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
            mpc.set_steering(self.steering)

class TimerState(GlobalThread):
    def __init__(self):
        self.last_timeout = timestamp()
        self.first_timeout = timestamp()
        pass

    def runner(self):
        log_info("In TimerState::runner")
        while running:
            time.sleep(1.0)
            if (timestamp() - vc.last_received_timestamp) > LAN_TIMEOUT and vc.wan_status != WAN_LAN_OUTAGE:
                log_critical("LAN TIMEOUT DETECTED")
                with vc.lock:
                    vc.wan_status = WAN_LAN_OUTAGE
                    vc.state = STATE_SAFETY_DRIVER
                self.last_timeout = timestamp()
                self.first_timeout = self.last_timeout
            if (timestamp() - self.last_timeout) > 5_000_000 and vc.wan_status == WAN_LAN_OUTAGE:
                log_info(f"Continued LAN Outage for {(timestamp() - self.first_timeout)//1000_000} seconds")
                self.last_timeout = timestamp()
    # end of runner
# end of TimerState

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

###############################################################################################
# Unit test bench code
###############################################################################################
class FileWriter ():
    def __init__(self, file_path):
        self.file_path = file_path

    def save_message(self, message):
        """Appends a message (dictionary format) to the file."""
        with open(self.file_path, 'a') as file:
            file.write(json.dumps(message) + '\n')
    # end of save_message
# end of FileWriter

import random

def time_based_state_generator(state_list, duration_in_s_list):
    """Generator that rotates through the state_list spending time in each per
        the duration_list
    """
    last_switch_time = time.time() + (offset:= random.uniform (1, 5))
    current_state_index = 0
    while True:
        duration_in_current_state = time.time () - last_switch_time
        # print (f'duration in current_state: {duration_in_current_state:.2f}')
        duration_expired = duration_in_current_state > \
            duration_in_s_list[min (current_state_index, len (duration_in_s_list)-1)] 
        if duration_expired:
            current_state_index = (current_state_index + 1) % len (state_list)
            last_switch_time = time.time ()
        yield state_list[current_state_index]
# end of time_based_state_generator

mph_to_MKS = lambda x: x*1609.34/3600

class OPStateUnitTest(GlobalThread):
    def __init__(self):
        self.enabled = False
        self.speed = 0
        self.steering = 0
        self.accelerator_override = False
        self.steering_override = False
        self.brake_override = False
        self.last_enabled = False
        self.last_status = timestamp()

        self.speed_gen = time_based_state_generator (
            [mph_to_MKS(20), mph_to_MKS(50)], duration_in_s_list=[5, 1])
        self.current_enable_gen = time_based_state_generator (
            [False, True], duration_in_s_list=[0.5, 5])
        self.accelerator_override_gen = time_based_state_generator (
            [True, False], duration_in_s_list=[0.5, 5])
        self.brake_override_gen = time_based_state_generator (
            [True, False], duration_in_s_list=[0.5, 5])
        self.steering_override_gen = time_based_state_generator (
            [True, False], duration_in_s_list=[0.5, 5])
    # end of __init__

    def override(self):
        return self.accelerator_override or self.steering_override or self.brake_override
    
    def runner(self):
        log_info("OpState: runner")
        while running:
            # wake up ever 10Hz and mostly randomly update states
            time.sleep (0.1)

            self.speed = round (next (self.speed_gen), 2)
            self.steering = vc.steering
            self.enabled = next (self.current_enable_gen)
            self.accelerator_override = next (self.accelerator_override_gen)
            self.brake_override = next (self.brake_override_gen)
            self.steering_override = next (self.steering_override_gen)

            if self.enabled != self.last_enabled:
                log_info(f"OP.Enabled {self.last_enabled}->{self.enabled}")
                self.last_enabled = self.enabled

            if (timestamp() - self.last_status) > 5_000_000:
                self.last_status = timestamp()
                gas = "GasPressed" if self.accelerator_override else ""
                brk = "BrakePressed" if self.brake_override else ""
                log_info(f"STATUS: State:{vc.state} WAN:{vc.wan_status} OP_Enabled:{self.enabled} Request:{vc.request_enable} Speed:{self.speed} Steering:{self.steering} {gas} {brk}")
        # end of while running
    # end of runner
# end of UnitTestOPstate

def stop_tests ():
    global running
    running = False

###############################################################################################

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
        vc.start(daemon=True)
        timer.start(daemon=True)
        op.start(daemon=True)
        mpc.start(daemon=True)
   
    def hijack(self, accel, CS):
        '''Openpilot calls this function on every iteration of the PID controller when enabled. '''
        # Some car events show up in CS, not in messages...
        op.accelerator_override = CS.gasPressed
        op.brake_override = CS.brakePressed
        op.steering_override = CS.steeringPressed

        if vc.state != STATE_REMOTE_DRIVER:
            # Reflect back the input, i.e., no change
            return accel
        else:
            # override the controls
            return vc.acceleration
        
class MPCController(GlobalThread):
    '''Interface to legacy MPC controller'''
    def __init__(self, unit_test):
        self.curvature = None
        self.socket = None
        self.unit_test = unit_test
        self.last_good_read_time = timestamp()
        self.request_tag = 0

    def runner(self):
        if self.unit_test:
            while running:
                time.sleep(.5)
            return
        
        while running:
            if vc.wan_status != WAN_NORMAL:
                if self.socket is not None:
                    log_info(f"Closing MPC socket due to WAN State of {vc.wan_status}")
                    self.socket.close()
                self.socket = None
                self.curvature = None
            else:
                try:
                    if self.socket is None:
                        log_info(f"Creating connection to MPC Controller")
                        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                        self.socket.setblocking(False)
                        self.socket.connect(('127.0.0.1', 6388))
                    result = self.socket.recv(1024)
                    self.last_good_read_time = timestamp()
                    if result:
                        log_info(f"Got MPC Response {result.decode()}")
                    else:
                        log_critical("MPC Closed connection")
                        self.socket = None
                except BlockingIOError:
                    '''Socket read timed out. ignore'''
                    time.sleep(.25)
                except ConnectionError:
                    '''Probably the MPC got shutdown'''
                    self.socket = None
                except ConnectionRefusedError:
                    if timestamp() - self.last_good_read_time > 10000:
                        log_info(f"Connection Refused to MPC Controller for {(timestamp()-self.last_good_read_time)/1000} Secs")
    
    def set_steering(self, curvature):
        if self.socket is not None and curvature != self.curvature:
            if math.isinf(curvature):
                radius = 0
            elif curvature == 0:
                radius = 10000000000000.0
            else:
                radius = 1 / curvature
            #
            # Make message in format for MPC controller
            #
            self.request_tag = self.request_tag + 1
            mpc_msg = f"<{self.request_tag}>c {-radius}\r\n"
            #print(f"Sending trajectory radius {-radius}")
            try:
                self.socket.send(mpc_msg.encode('utf-8'))
                self.curvature = curvature
            except:
                log_critical(f"Unable to send to MPC controller")




def do_unit_tests():

    pass

if __name__ == "main":
    # Invoked from command line, this is the unit test case.
    do_unit_tests()
else:
    # Loaded, meaning this is production usage in OpenPilot
    vc = VCState()
    timer = TimerState()
    op = OPState() if 'VC_UNIT_TEST' not in os.environ else OPStateUnitTest ()
    mpc = MPCController('VC_UNIT_TEST' in os.environ)
    
    # instantiate log file writer if running unit tests
    if 'VC_UNIT_TEST' in os.environ:
        tx_log_path = f'{os.environ["VC_UNIT_TEST"]}_vc_tx.json'
        if os.path.exists (tx_log_path): os.remove (tx_log_path)
        tx_file_writer = FileWriter (tx_log_path)

        rx_log_path = f'{os.environ["VC_UNIT_TEST"]}_vc_rx.json'
        if os.path.exists (rx_log_path): os.remove (rx_log_path)
        rx_file_writer = FileWriter (rx_log_path)