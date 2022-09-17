-- This has not be checked with the Postgres, so there may be some SQL errors.

-- The data model has two parts:

-- 1. Video data points are uniquely identified by (video_session.id,
-- transmit timestamp) for video, and GNSS data points are uniquely identified
-- by (gnss_session.id, measurement timestamp).

-- 2. The real-world correspondence between video data and GNSS data
-- is expressed as a relation (video_session.id, gnss_session.id),
-- which asserts that video and GNSS data with these ids were
-- collected on the same real-world trajectory and that the timestamps
-- can be used for approximate alignment.  A single drive can comprise
-- multiple sessions.

-- The basic objects comprise comm sources and receivers, GNSS
-- receivers, cellular carriers, video sessions, and GNSS sessions.

-- Timestamps are stored to millisecond precision or better.

-- This table describes sources (normally vehicles). A new row with a
-- new id should be entered whenever we change the configuration of
-- the vehicle software or hardware should create a new row
CREATE TABLE comm_source (
  id VARCHAR(32),
  -- other attributes TBD, e.g. modem configuration
  PRIMARY KEY(id)
)

-- This table describes receivers (normally operator stations). A new
-- row with a new id should be entered whenever we change the
-- configuration of the vehicle software or hardware should create a
-- new row
CREATE TABLE comm_receiver (
  id VARCHAR(32)
  -- other attributes TBD, e.g. station identifier
  PRIMARY KEY(id)
)

-- This table describes GiNSS receivers. A new row with
-- a new id should be entered whenever we change the configuration of
-- the GNSS, i.e. from one chip set to another, changing antenna.
CREATE TABLE gnss_receiver (
  id VARCHAR(32)
  -- other attributes TBD, e.g. chipset or breakout board, antenna
  PRIMARY KEY(id)
)

-- This table identifies carriers.
CREATE TABLE cellular_carrier (
  id VARCHAR(32),
  -- e.g. "T-Mobile commercial, IMSI 869710030002905"
  display_text TEXT,
  PRIMARY KEY(id)
)

-- This table describes a video session.  A session models the video
-- data from a real-world trajectory.  There may be many video clips
-- that belong to a single video_session id.
CREATE TABLE video_session (vi
  id VARCHAR(32) NOT NULL, -- e.g UUID
  sender VARCHAR(32),
  receiver VARCHAR(32),
  -- Informal description, such as "low-speed urban experiment #1"
  description TEXT,
  -- other attribues TBD
  PRIMARY KEY(id),
  FOREIGN KEY(sender)
    REFERENCES(comm_source, id),
  FOREIGN KEY(receiver)
    REFERENCES(comm_receiver, id)
)

--  This table describes a GNSS session.  A session models the
--  coordinates of part of all of a real-world trajectory.  There may
--  be many files generated by a GNSS receiver that belong to a single
--  gnss_session id.
   CREATE TABLE gnss_session (
   id VARCHAR(32) NOT NULL, -- e.g UUID
   receiver VARCHAR(32),
  -- Informal description, such as "low-speed urban experiment #1"
  description TEXT,
  -- other attributes TBD
  PRIMARY KEY(id),
  FOREIGN KEY(sender)
    REFERENCES(gnss_receiver, id)
)

-- video transfer metrics.

CREATE TABLE  video_transfer_metrics (
  video_session VARCHAR(32),
  transmit TIMESTAMP WITH TIME ZONE,
  bitrate INTEGER, -- kbps
  latency INTEGER, -- ms
  PRIMARY KEY(video_session, transmit),
  FOREIGN KEY(video_session)
    REFERENCES (video_session, id)
)

-- bitrate supplied by carrier

CREATE TABLE  carrier_usage_metric (
  sender VARCHAR(32),
  receiver VARCHAR(32),
  transmit TIMESTAMP WITH TIME ZONE,
  carrier VARCHAR(32),
  bitrate INTEGER, -- kbps
  PRIMARY KEY(sender, receiver, transmit),
  FOREIGN KEY(carrier)
    REFERENCES(cellular_carrier, id)
)

-- scene complexity
CREATE TABLE scene_complexity (
  video_session VARCHAR(32),
  transmit TIMESTAMP WITH TIME ZONE,
  complexity REAL,
  PRIMARY KEY(video_session, transmit)
  FOREIGN KEY(video_session)
    REFERENCES (video_session, id)
)

-- video session file information
CREATE TABLE video_clip (
  video_session VARCHAR(32),
  -- partial path to file.
  -- this is the part that is independent of the physical storage.
  -- e.g. /mnt/4TB/video_clips/experimental/ + file_name
  file_name TEXT,
  -- [start, end)
  start TIMESTAMP WITH TIME ZONE,
  end TIMESTAMP WITH TIME ZONE,
  -- other attribues TBD, e.g. encoding or resolution.
  PRIMARY KEY(video_session, file_name),
  FOREIGN KEY(video_session)
    REFERENCES (video_session, id)
)

-- GNSS session file information
CREATE TABLE gnss_track (
  id VARCHAR(32),
  gnss_session VARCHAR(32),
  -- partial path to file.
  -- this is the part that is independent of the physical storage.
  -- e.g. /mnt/4TB/gnss_tracks/experimental/ + file_name
  file_name VARCHAR,
  -- other attribues TBD, e.g. device or encoding
  PRIMARY KEY(id),
  FOREIGN KEY(gnss_session)
    REFERENCES (gnss_session, id),
  UNIQUE(gnss_session, file_name)
)

-- GNSS data as PostGIS ST_Point with key track + measurement timestamp
-- This allows joining with other GIS, such as OSM data to find street names.
CREATE TABLE gnss_point (
   track VARCHAR(32),
   measured TIMESTAMP WITH TIME ZONE,
   point GEOMETRY, -- ST_Point
   PRIMARY KEY (track, measured)
   FOREIGN KEY(track)
     REFERENCES(gnss_track, id),
)
