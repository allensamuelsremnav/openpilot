```gpsdconcat``` takes raw minute-by-minute gpsd log and synthesize single gpsd log for a session.

# Theory
*  Session boundaries are decided on the receiver and not send to the vehicle.
* ```gpsdol``` runs on the vehicle (rn5) and writes minute-by-minute logs.
* At the end of the data, the raw logs are rsync'd to a directory specific to the vehicle
* ```gpsdconcat``` inspects the metadata for a session (<sessionid>/video/<metadata.csv>) and finds the sender time stamps
* ```gpsdconcat``` then concatenates the gnss logs that intersect the time range spanned by the sender time stamps and writes to <sessionid>/gnss

# See test.sh for sample invocation.

