This `README` might not match the code; the code is authoritative.

# Command flow

Run these commands in this order in separate Powershell windows.

1. `d:/remnav_bin/gpsdlisten.exe`
2. `d:/remnav_bin/plnr.exe`
3. `AppDecD3d.exe`
4. `d:/video_decoder/utils/udp_sender/runit.bat`

# Inputs and outputs.

* g920 (input).  Read reports using the HID interface identified by a
  pair of integers `vid:pid`.

* TPV (input).  Read TPV messages from `gpsdlisten` using a local port.

* vehicle (input/output).  Send trajectory requests and g920 pedal state
  and read trajectory-application messages using a bidirectional UDP
  channel.

* display (output).  Send trajectory requests and
  trajectory-application messages to the `AppDecD3D`
  display application using a local port.

Use `-help` to see the switches and default values for these inputs
and outputs.

The `plnr` application can mock g920, TPV, or vehicle; see `-help` for
the mock values for the switches.

# Log files

Program outputs are logged for debugging.

* d:/remnav_log/trajector/\<uniqueid\>/YYYYmmddTHHMMZ
* d:/remnav_log/vehiclecmds/\<uniqueid\>/YYYYmmddTHHMMZ

The uniqueid is computed and displayed when the application starts.

# Build the application

1. Follow the official golang instructions (https://go.dev/doc/install) for installing `go`.

2. Clone the `remnav/integration/rn1` repository from bitbucket.

3. Run the command `go build` in the directory `rn1/remnav/cmd/plnr`.

