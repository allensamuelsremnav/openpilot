```log2csv``` converts the
 * ```vehiclecmds``` log files to g920.csv and traj.csv or
  * ```trajectory``` log files to trajapplication.csv and traj.csv.

  These log files are stored under the log root used for running the planner.

# Plotjuggler

For logs in the ```vehiclemds```
* Open PlotJuggler
* Use the ```Data``` widget to select the ```g920.csv```.
* Use ```requested``` for the timeseries axis.
* Clear previous data.
* Use the ```Data``` widget to select the ```traj.csv```.
* Do not clear previous data.
* Show ```wheel``` and ```curvature``` on the same plot.

For the ```trajectory``` logs, you are likely to want to see just the delays inferred from TrajectoryApplication data.  For convenience, a ```delay``` field is included in the ```csv``` file.

* Open the ```trajapplication.csv``` file.
* Use ```requested``` for the time.
* Show ```delay```.