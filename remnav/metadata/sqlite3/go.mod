module remnav/metadata/sqlite3

go 1.19

replace remnav/metadata/storage => ../storage

require (
	github.com/mattn/go-sqlite3 v1.14.15
	remnav/metadata/storage v0.0.0-00010101000000-000000000000
)
