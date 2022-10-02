# Metadata for experiments with video prototype



## Files and directories ##

### ```schema.sql```
The schema for metadata in a PostgreSQL database.

### ```experiment/experiment.json``` This is a model configuration
file for information needed at the vehicle and operator station.  It
is read by the launcher, gnss, and video applications.

Setting up an experiment should include creating the configuration
file for the experiment, copying it to the vehicle and operator
station, and passing the filename to the applications.

