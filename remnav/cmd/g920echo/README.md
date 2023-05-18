Sample program to use bidirectional channel to get G920 reports.

# ```g920echo```

This program reads G920 reports from the operator station and sends
heartbeats to keep the bidirectional channel alive.  It is a model for
Python or C++ programs that want to use bidrectional UDP from the
vehicle.

It is similar to ```trjecho```, but simpler: the vehicle-->
operator direction is only used for heartbeats.

See ```rn1/remnav/cmd/trjecho/README.md``` for information about the
three-component architecture and conventional port assignments.

# G920 report structure

See ```rn1/remnav/g920/g920.go``` for an example decoding function and
constants for indices in the array of bytes in the report.
