package storage

import (
	"encoding/json"
	"log"
)

type IdsInit struct {
	VideoSource         []IdDesc `json:"video_source"`
	VideoDestination    []IdDesc `json:"video_destination"`
	Cellular            []IdDesc `json:"cellular"`
	GNSSReceiver        []IdDesc `json:"gnss_receiver"`
	VideoPacketFormat   []IdDesc `json:"video_packet_format"`
	VideoMetadataFormat []IdDesc `json:"video_metadata_format"`
	GNSSTrackFormat     []IdDesc `json:"gnss_track_format"`
}

// This a JSON string for easy updates by non-golang users.
var idsJSON = `{
    "video_source": [
	{"id": "rn5_000",
	 "description": "undefined configuration"}
    ],

    "video_destination": [
	{"id": "rn3_000",
	 "description": "undefined configuration"}
    ],

    "cellular": [
	{"id": "att000",
	 "description": "undefined configuration"},
	{"id": "tmobile000",
	 "description": "undefined configuration"},
	{"id": "verizon000",
	 "description": "undefined configuration"}
    ],
    
    "gnss_receiver": [
	{"id": "NEO-6M",
	 "description": "https://www.amazon.com/dp/B07P8YMVNT"
	}
    ],

    "video_packet_format": [
	{"id": "empty",
	 "description": "empty file; use for testing storage traversal"}
    ],

    "video_metadata_format": [
	{"id": "empty",
	 "description": "empty file; use for testing storage traversal"}
    ],
    
    "gnss_track_format": [
	{"id": "empty",
	 "description": "empty file; use for testing storage traversal"}
    ]
}`

func InitialIds() IdsInit {
	// Return the documented ids.  The database generator will
	// add information for undocumented ids.
	if !json.Valid([]byte(idsJSON)) {
		log.Fatal("programming error, invalid idsJSON")
	}
	var ret IdsInit
	json.Unmarshal([]byte(idsJSON), &ret)
	return ret
}
