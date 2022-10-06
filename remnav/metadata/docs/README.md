# Setting up vehicle and archive station


## Vehicle (rn5)
* Create vehicle_user on vehicle.  User should be able to rsync to rn3.
* Make a directory on rn5:/home/<vehicle_user>/remconnect
* Add experiment.json to this directory
* Make `vehicle_root` directory; make group writable.

## Archive (rn3)
* Create `archive_root` directory, e.g. `/home/user/6TB/remconnect`. Make group writable.
* Install recent version of sqlite3.  If `SELECT unixepoch('2022-10-04');` fails, install a later version from [sqlite.org](https://www.sqlite.org/download.html)

## experiment.json
* Make a copy of [experiment.json](https://bitbucket.org/remnav/rn1/src/master/remnav/metadata/experiment/experiment.json)
* Verify that `video_source`, `video_destination`, and `gnss_receiver` match the experimental hardware and software. Verify corresponding entries in [idsJSON](https://bitbucket.org/remnav/rn1/src/master/remnav/metadata/storage/ids.go)
* Set `video_sender` and `gnss_client` to the full file paths.
* Verify `archive_root`, `archive_sender`, and `vehicle_root`.
