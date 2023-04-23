# Generated by portassignments.py


# operator station, external
#
# GPSD messages from vehicle gpsdrt to the operator.
OPERATOR_GPSD_LISTENER = 6001

# Trajectory-applied messages from trajectory execution to operator.
OPERATOR_TRAJECTORY_LISTENER = 6002


# operator station, localhost
#
# GPSD messages from the GPSD listener to display.
OPERATOR_GPSD_DISPLAY = 7000

# GPSD messages from the GPSD listener to trajectory planner.
OPERATOR_GPSD_TRAJECTORY = 7001

# Trajectory messages from trajectory planner to trajectory listener for forwarding.
OPERATOR_TRAJECTORY_REQUEST = 7002

# Trajectory-applied messages from trajectory listener to display.
OPERATOR_TRAJECTORY_APPLICATION = 7003


# vehicle, external
#

# vehicle, localhost
#
# Trajectory requests from trajectory dialer to trajectory execution
VEHICLE_TRAJECTORY_REQUEST = 7000

# trajectory-applied messages from trajectory execution to trajectory dialer for forwarding.
VEHICLE_TRAJECTORY_APPLICATION = 7001

