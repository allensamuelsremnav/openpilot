import socket, threading, json, time

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('', 0))
dest_address=(('172.25.203.145', 7777))

def listener():
    while True:
        o, address = s.recvfrom(1500)
        print(f"From {address}: {o}")

x = threading.thread(target=listener)

accel = 0
steer = 0
wan_status = 'normal'
request_enable = False

def timestamp():
    return int(time.time() * 1000)
message_id = 0

while True:
    line = input(">")
    if input[0] == 'u':
        accel = accel + .1
    elif input[0] == 'd':
        accel = accel - .1
    elif input[0] == 'l':
        steer = steer - .1
    elif input[0] == 'r':
        steer = steer + .1
    elif input[0] == 'E':
        request_enable = True
    elif input[0] == 'D':
        request_enable = False
    else:
        pass

    # Build a message
    message_id = message_id + 1

    msg = {
        'timestamp': timestamp(),
        'message_id': message_id,
        'acceleration': accel,
        'steering': steer,
        'wan_status': wan_status,
        'request_enable': request_enable,
    }

    s.sendto(dest_address, json.dumps(msg))
    print("Sending: {json.dumps(msg)}")
