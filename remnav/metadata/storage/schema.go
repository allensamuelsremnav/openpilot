package storage

var schema = []string{
	`-- The data model has two parts:

-- 1. Video data points are uniquely identified by (video_session.id,
-- transmit timestamp) for video, and GNSS data points are uniquely identified
-- by (gnss_session.id, measurement timestamp).

-- 2. The real-world correspondence between video data and GNSS data
-- is expressed as a share video_and gnss session id,
-- which asserts that video and GNSS data with these ids were
-- collected on the same real-world trajectory and that the timestamps
-- can be used for approximate alignment.  A single drive can comprise
-- multiple sessions.

-- The basic objects comprise comm senders and destination, GNSS
-- receivers, video sessions, and GNSS sessions.

-- Timestamps are stored to second precision or better.

-- This table describes sources (normally vehicles). A new row with a
-- new id should be entered whenever we change the configuration of
-- the vehicle software or hardware should create a new row
CREATE TABLE video_source (
  id VARCHAR(32),
  -- Informal description, such as
  -- "video pipeline, 1 camera"
  description TEXT,
  -- other attributes TBD, e.g. modem configuration
  PRIMARY KEY(id)
);
`,
	`
-- This table describes destinations (normally operator stations). A new
-- row with a new id should be entered whenever we change the
-- configuration of the vehicle software or hardware should create a
-- new row
CREATE TABLE video_destination (
  id VARCHAR(32),
  -- Informal description, such as
  -- "no error concealment"
  description TEXT,
  -- other attributes TBD, e.g. station identifier
  PRIMARY KEY(id)
);
`,
	`
-- This table defines cellular connfigurations.
-- A video session using three modems will use
-- three values from this table.
CREATE TABLE cellular (
  id VARCHAR(32),
  -- Informal description, such as
  -- "ATT sim, IMSI 869710030002905, TN 14088165141"
  description TEXT,
  -- other attributes TBD, e.g. station identifier
  PRIMARY KEY(id)
);
`,
	`
-- This table describes GNSS receivers. A new row with
-- a new id should be entered whenever we change the configuration of
-- the GNSS, i.e. from one chip set to another, changing antenna.
CREATE TABLE gnss_receiver (
  id VARCHAR(32),
  -- Informal description, such as
  -- "Amazon M6"
  description TEXT,
  -- other attributes TBD, e.g. chipset or breakout board, antenna
  PRIMARY KEY(id)
);
`,
	`
-- This table describes a video session.  A session models the video
-- data from a real-world trajectory.  There may be many video clips
-- that belong to a single video_session id.
CREATE TABLE video_session (
  id VARCHAR(128) NOT NULL, -- e.g UUID
  source VARCHAR(32) REFERENCES video_source(id),
  destination VARCHAR(32) REFERENCES video_destination(id),
  -- Informal description, such as "low-speed urban experiment #1"
  description TEXT,
  -- other attribues TBD
  PRIMARY KEY(id)
);
`,
	`
--  This table describes a GNSS session.  A session models the
--  coordinates of part of all of a real-world trajectory.  There may
--  be many files generated by a GNSS receiver that belong to a single
--  gnss_session id.
CREATE TABLE gnss_session (
   id VARCHAR(128) NOT NULL, -- e.g UUID
   receiver VARCHAR(32) REFERENCES gnss_receiver(id),
   -- Informal description, such as "low-speed urban experiment #1"
   description TEXT,
   -- other attributes TBD
   PRIMARY KEY(id)
);
`,
	`
-- format for files with video packets
--
-- the id is used to choose file readers in analysis languages.
-- the file reader will know, for example, that the data is in binary
-- format, whether there is a header, and the data types and units.
CREATE TABLE video_packets_format (
  id VARCHAR(32),
  -- informal description"
  description TEXT,
  PRIMARY KEY(id)
);

`,
	`
-- the id is used to choose file readers in analysis languages.
-- the file reader will know, for example, that the data is in CSV
-- format, whether there is a header, and the data types and units.
CREATE TABLE video_metadata_format (
  id VARCHAR(32),
  -- informal description, such as "sequence, skip, tx epoch ms, rx epoch m"
  description TEXT,
  PRIMARY KEY(id)
);
`,
	`
-- files with video transfer measurements, such as epoch + latency
-- or epoch + carrier kbps
CREATE TABLE video_metadata (
  video_session VARCHAR(128) REFERENCES video_session(id),
  filename TEXT,
  start_time TIMESTAMP WITH TIME ZONE,
  cellular VARCHAR(32) REFERENCES cellular(id),
  format VARCHAR(32) REFERENCES video_metadata_format(id),
  -- other attribues TBD, e.g. encoding or resolution.
  PRIMARY KEY(video_session, filename)
);
`,
	`
-- video session file information
CREATE TABLE video_packets (
  video_session VARCHAR(128) REFERENCES video_session(id),
  -- partial path to file.
  -- this is the part that is independent of the physical storage.
  -- e.g. /mnt/4TB/video_clips/experimental/ + filename
  filename TEXT,
  -- start_time is encoded in the filename
  start_time TIMESTAMP WITH TIME ZONE,
  cellular VARCHAR(32) REFERENCES cellular(id),
  -- format is conventionally encoded in file name
  format VARCHAR(32) REFERENCES video_packets_format(id),
  -- other attribues TBD, e.g. encoding or resolution.
  PRIMARY KEY(video_session, filename)
);
`,
	`
-- format for files with GNSS tracks
--
-- the id is used to identify file readers in analysis languages.
-- the file reader will know, for example, that the data is in 
-- NMEA format, which NMEA sentences are present, and the time
-- representation -- UTC, GPS clock, etc.
CREATE TABLE gnss_track_format (
  id VARCHAR(32),
  -- informal description, such as "gpx schema https://blah-blah/bl"
  description TEXT,
  PRIMARY KEY(id)
);
`,
	`
-- GNSS session file information
CREATE TABLE gnss_track (
  gnss_session VARCHAR(128) REFERENCES gnss_session(id),
  -- partial path to file.
  -- this is the part that is independent of the physical storage.
  -- e.g. the file is stored at
  -- rn1:/mnt/4TB/gnss_tracks/experimental/ + <gnss_session>/filename
  filename TEXT,
  format VARCHAR(32) REFERENCES gnss_track_format(id),
  start_time TIMESTAMP WITH TIME ZONE,
  -- other attribues TBD, e.g. device or encoding
  PRIMARY KEY(gnss_session, filename)
);
`}

func Schema() []string {
	return schema
}
