Sample programs to use bididirectional channel to get trajectory
requests and send trajectory-applied messages.

# Running a mock environment

## ```remnav/cmd/plnr/plnr.exe```
The trajectory planner can be configured to use gpsd and g920 mocks
with a live bidi channel to the vehicle: use ```--tpv_port 0 -vidpid 0```

## ```bidiwr```
This is a shim that makes the bidirectional UDP channel accessible via
a socket connection on the vehicle.  It also deduplicates received messages.

* Build Linux executable in ```rn1/remnav/cmd/bidiwr/```.
* Start ```./bidiwr```. Use ```--port``` to specify the local port,
  ```--dest``` to specify the destination ```address:port```, and
  ```recvKey``` to specify the field for deduplication of received
  messages by time.

```--verbose``` is useful for debugging.

## ```trjecho```

This program reads trajectories and responds with
trajectory-applied messages.  It uses a local port (provided by ```bidiwr```).  It
is a model for Python or C++ programs that want to use bidirectional
UDP from the vehicle.

* Build linux executable in ```rn1/remnav/cmd/trjecho/```
* Start ```./trjecho  --port 7000```

```--port``` should match the ```--port``` switch on ```bidiwr```.

```--progress``` is useful for debugging.  In normal operation, you
will see a sequence of `a`'s and `t`'s indicating that applied
(trajectory) messages have been sent (received).

## Restarts

```plnr.exe``` > ```bidiwr``` > ```trjecho```

```x > y``` means "restart y after restarting x".

## Port assignments

Language-specific files for port assignments are in
```rn1/remnav/net/port.py```, ```port.h```, and ```port.go```.
