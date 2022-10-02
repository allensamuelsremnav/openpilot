package experimentconfig

// Structs for experiment configuration JSON.

type VideoStorage struct {
	// Root directory for archive storage at operator station,
	// e.g. /home/user/6TB/remconnect/video
	Root string `json:"root"`
}

type VideoConfig struct {
	// Identifiers for source and destination, e.g. "proto000"
	// and "rn3_000"
	VideoSource      string `json:"video_source"`
	VideoDestination string `json:"video_destination"`

	// Executable
	VideoSender string `json:"video_sender"`
	Storage     VideoStorage
}

type GNSSStorage struct {
	// Root directories on vehicle and archive server.
	VehicleRoot string `json:"vehicle_root"`
	ArchiveUser string `json:"archive_user"`
	ArchiveRoot string `json:"archive_root"`
}

type GNSSConfig struct {
	// Identify the GNSS receiver used, e.g. "Neo-6M"
	GNSSReceiver string `json:"gnss_receiver"`

	// Executable
	GNSSClient string `json:"gnss_client"`
	Storage    GNSSStorage
}

// Top-level JSON
type Config struct {
	Video VideoConfig
	GNSS  GNSSConfig
}
