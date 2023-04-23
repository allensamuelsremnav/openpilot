package net

// Generated by portassignments.py

//
// operator station, external
//
// GPSD messages from vehicle gpsdrt to the operator.
const OperatorGPSDListener = 6001

// Trajectory-applied messages from trajectory execution to operator.
const OperatorTrajectoryListener = 6002

//
// operator station, localhost
//
// GPSD messages from the GPSD listener to display.
const OperatorGPSDDisplay = 7000

// GPSD messages from the GPSD listener to trajectory planner.
const OperatorGPSDTrajectory = 7001

// Trajectory messages from trajectory planner to trajectory listener for forwarding.
const OperatorTrajectoryRequest = 7002

// Trajectory-applied messages from trajectory listener to display.
const OperatorTrajectoryApplication = 7003

//
// vehicle, external
//
//
// vehicle, localhost
//
// Trajectory requests from trajectory dialer to trajectory execution
const VehicleTrajectoryRequest = 7000

// trajectory-applied messages from trajectory execution to trajectory dialer for forwarding.
const VehicleTrajectoryApplication = 7001

