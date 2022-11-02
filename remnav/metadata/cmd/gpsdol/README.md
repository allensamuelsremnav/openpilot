This is the offline GNSS_CLIENT.

# `gpsdol`

Use `-h` to see the switches.

`go build .` creates the executable.

# Listen on the LAN

We need to set up `gpsd` to listen on the LAN, not just localhost.

## `gpsd.socket`

Put `gpsd.socket` in `/etc/systemd/system` on the raspberry pi that is running `gpsd`.  You may need to reboot or restart the gpsd service.

This alternative worked on the rapsberry pi on `rn3`.

## `gpsd.service`



Add `-G` to the `ExecStart` in `/lib/systemd/system/gpsd.service` on the raspberry pi.


```
[Unit]
Description=GPSd daemon service file

[Service]
Type=forking
User=root
Group=dialout
TimeoutStartSec=0
ExecStart=/usr/local/sbin/gpsd -n /dev/ttyACM0 /dev/pps0 -G  -F /var/run/gpsd.sock

# Grouping mechanism that let systemd start groups of processes up at the same time
WantedBy=multi-user.target
```

FC was unable to get the `gpsd.socket` change to work on `rn5` and used this alternative.

