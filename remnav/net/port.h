#ifndef REMNAV_NET_PORTS_H_
#define  REMNAV_NET_PORTS_H_
// Generated by portassignments.py.


// operator station
//
// Gpsd messages from vehicle gpsdrt to the operator.
const int32_t OPERATOR_GPSD_LISTEN = 6001;

// Bidi listener for trajectories and trajectory-applied messages.
const int32_t OPERATOR_TRAJECTORY_LISTEN = 6002;

// Unused, available.
const int32_t OPERATOR_UNUSED_AVAILABLE = 7000;

// Gpsd messages from the gpsd listener to trajectory planner. localhost
const int32_t OPERATOR_GPSD_TRAJECTORY = 7001;

// Trajectory messages from trajectory planner to trajectory listener for forwarding. localhost
const int32_t OPERATOR_TRAJECTORY_REQUEST = 7002;

// Trajectory-applied messages from trajectory listener to display. localhost
const int32_t OPERATOR_TRAJECTORY_APPLICATION = 7003;

// Overlay messages from decoder and gpsd listener to operator (display).
const int32_t OPERATOR_OVERLAY_LISTEN = 7777;

// Decoded video messages from decoder to operator (display).
const int32_t OPERATOR_VIDEO_LISTEN = 8888;


// vehicle
//
// Trajectory requests from trajectory dialer to trajectory execution; trajectory-applied messages from trajectory execution to trajectory dialer for forwarding. localhost
const int32_t VEHICLE_TRAJECTORY_REQUEST_APPLICATION = 7000;

#endif  // REMNAV_NET_PORTS_H_
