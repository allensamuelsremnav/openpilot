#ifndef REMNAV_NET_PORTS_H_
#define  REMNAV_NET_PORTS_H_
// Generated by portassignments.py.


// operator station, external
//
// Gpsd messages from vehicle gpsdrt to the operator.
const int32_t OPERATOR_GPSD_LISTENER = 6001;

// Trajectory-applied messages from trajectory execution to operator.
const int32_t OPERATOR_TRAJECTORY_LISTENER = 6002;


// operator station, localhost
//
// Gpsd messages from the gpsd listener to display.
const int32_t OPERATOR_GPSD_DISPLAY = 7000;

// Gpsd messages from the gpsd listener to trajectory planner.
const int32_t OPERATOR_GPSD_TRAJECTORY = 7001;

// Trajectory messages from trajectory planner to trajectory listener for forwarding.
const int32_t OPERATOR_TRAJECTORY_REQUEST = 7002;

// Trajectory-applied messages from trajectory listener to display.
const int32_t OPERATOR_TRAJECTORY_APPLICATION = 7003;


// vehicle, external
//

// vehicle, localhost
//
// Trajectory requests from trajectory dialer to trajectory execution
const int32_t VEHICLE_TRAJECTORY_REQUEST = 7000;

// trajectory-applied messages from trajectory execution to trajectory dialer for forwarding.
const int32_t VEHICLE_TRAJECTORY_APPLICATION = 7001;

#endif  // REMNAV_NET_PORTS_H_
