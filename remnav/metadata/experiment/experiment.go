package experimentconfig

// Structs for experiment-configuration JSON.

type StorageConfig struct {
	// Server and root directory for archive storage at operator station,
	// e.g. 96.64.247.70 and /home/user/6TB/remconnect
	ArchiveServer string `json:"archive_server"`
	ArchiveRoot   string `json:"archive_root"`
	// Root directory for local storage
	VehicleRoot string `json:"vehicle_root"`
}

type VideoConfig struct {
	// Identifiers for source and destination configurations,
	// e.g. "proto000" and "rn3_000"
	VideoSource      string `json:"video_source"`
	VideoDestination string `json:"video_destination"`

	// Full path to executable
	VideoSender string `json:"video_sender"`
}

type GNSSConfig struct {
	// Identifier the GNSS receiver used, e.g. "Neo-6M with Ublox antenna"
	GNSSReceiver string `json:"gnss_receiver"`

	// Full path to executable
	GNSSClient string `json:"gnss_client"`
}

// Top-level JSON
type Config struct {
	Description string `json:"description"`
	Storage     StorageConfig
	Video       VideoConfig
	GNSS        GNSSConfig
}
