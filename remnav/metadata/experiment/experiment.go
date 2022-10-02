package experimentconfig

// Structs for experiment-configuration JSON.

type VideoStorage struct {
	// Root directory for archive storage at operator station,
	// e.g. /home/user/6TB/remconnect/video
	Root string `json:"root"`
}

type VideoConfig struct {
	// Identifiers for source and destination configurations,
	// e.g. "proto000" and "rn3_000"
	VideoSource      string `json:"video_source"`
	VideoDestination string `json:"video_destination"`

	// Full path to executable
	VideoSender string `json:"video_sender"`

	Storage     VideoStorage
}

type GNSSStorage struct {
	// Root directories for GNSS log storage on vehicle
	VehicleRoot string `json:"vehicle_root"`

	// These credentials will be used to rsync from the VehicleRoot
	// to ArchiveRoot on rn3
	ArchiveUser string `json:"archive_user"`
	// E.g. /home/user/6TB/remconnect/gnss
	ArchiveRoot string `json:"archive_root"`
}

type GNSSConfig struct {
	// Identifier the GNSS receiver used, e.g. "Neo-6M with Ublox antenna"
	GNSSReceiver string `json:"gnss_receiver"`

	// Full path to executable
	GNSSClient string `json:"gnss_client"`

	Storage    GNSSStorage
}

// Top-level JSON
type Config struct {
        Description string `json:"description"`
	Video VideoConfig
	GNSS  GNSSConfig
}
