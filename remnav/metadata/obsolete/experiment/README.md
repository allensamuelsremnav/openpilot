# Setting up a vehicle and archive server ("operator station")

## Vehicle (rn5)
* Create user <vehicle_user> on vehicle.  This user should be able to rsync to rn3.
* Make a directory for files created by the video sender and GNSS client applications, e.g. `rn5:/home/<vehicle_user>/remconnect`.  This will be identifed as `vehicle_root` in the experiment's JSON file.
+ Make the `vehicle_root` directory group writable.

## Archive (rn3)
* Create `archive_root` directory, e.g. `rn3:/home/user/6TB/remconnect`. Make group writable.
* Install a recent version of sqlite3.  If `SELECT unixepoch('2022-10-04');` fails, install a later version from [sqlite.org](https://www.sqlite.org/download.html)

# Setting up an experiment

Create a JSON file that describes the configuration.  The JSON file
should be based on `remnav/metadata/experiment/experiment.json`. It
can have any name; for exposition we assume that it is named
`experiment.json`.

See `experiment.go` for description of the fields in the experiment-configuration JSON.

* Verify that `video_source`, `video_destination`, and `gnss_receiver`
  match the experimental hardware and software.
* Verify corresponding entries in
  [ids.go](https://bitbucket.org/remnav/rn1/src/master/remnav/metadata/storage/ids.go)
* Verify the file format ids and cellular ids in [ids.go](https://bitbucket.org/remnav/rn1/src/master/remnav/metadata/storage/ids.go)
* Set `video_sender` and `gnss_client` to the full file paths.
* Verify that `archive_root`, `archive_sender`, and `vehicle_root`
  match the vehicle and archive server.
* Use the `description` field for a human-readable summary of the experiment.
* Place the edited `experiment.json` on the vehicle; it can have any name. (It will be passed to `rnlaunch`.)


