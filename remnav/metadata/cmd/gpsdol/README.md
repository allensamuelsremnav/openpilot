This is the offline GNSS_CLIENT.

# `gpsdol`

Use `-h` to see the switches.

`go build .` creates the executable.

# `gpsd.socket`

This `systemd` configuration sets up `gpsd` to listen on the LAN, not just localhost.

Put this in `/etc/systemd/system` on the raspberry pi that is running `gpsd`.  You may need to reboot or restart the gpsd service.
