# `dedup_root

Apply dedup executable to metadata and packet files for experiment sessions at `archive_root`.

```
dedup_root --dedup_prog ./dedup --archive_root /home/user/6TB/remconnect  --dedup_root /home/gopal.solanki/dedup --metadata_db /home/user/6TB/remconnect/metadata.db
```

Files under `archive_root` will be selected using the table `video_session` in `metadata.db`.  Output will be written under `dedup_root`.
Use `-help` to see additional switches.
