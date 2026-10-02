# KeyServe

KeyServe is a learning project for building a persistent key-value database in C++ from the ground up.

The project currently implements a durable, single-process local storage engine backed by an append-only binary log. The long-term goal is to evolve it into an authenticated TCP key-value server with concurrent clients and a more advanced disk-backed storage model.

## Current Features

- Persistent key-value storage
- Append-only binary database format
- `PUT`, `GET`, `DELETE`, and `EXISTS`
- Streaming database replay on startup
- CRC32 record integrity checking
- Automatic recovery from an incomplete final record
- Persistent POSIX file descriptors
- Reliable handling of partial reads/writes and `EINTR`
- Crash-safe database compaction
- Automatic compaction based on wasted disk space
- Manual `COMPACT` command
- File-permission preservation during compaction

## Building

Requirements:

- C++ compiler with C++17 support
- CMake
- Linux or WSL environment

The project is currently developed using:

- Ubuntu 24.04 under WSL2
- g++ 13.3
- CMake 3.28

Build with:

```bash
cmake -S . -B build
cmake --build build
```

Run with:

```bash
./build/keyserve
```

The database is currently stored as:

```text
keyserve.db
```

relative to the process working directory.

## CLI

Supported commands:

```text
PUT <key> <value>
GET <key>
DELETE <key>
EXISTS <key>
COMPACT
EXIT
```

Example:

```text
KeyServe> PUT name Alice
OK

KeyServe> GET name
name = Alice

KeyServe> EXISTS name
true

KeyServe> DELETE name
OK

KeyServe> GET name
NOT_FOUND
```

## Storage Model

KeyServe currently uses an append-only operation log.

Normal writes never modify existing records. A `PUT` or `DELETE` is serialized and appended to the database file before the in-memory state is changed.

This preserves the invariant:

```text
persistent state is updated before RAM state
```

At startup, the database log is replayed in order to reconstruct the current key-value state.

The current in-memory representation is:

```cpp
std::unordered_map<std::string, std::string>
```

A future storage milestone will replace in-memory values with record locations so that values remain on disk and `GET` operations can read them by offset.

## Database Format

The database begins with a five-byte header:

```text
"KSDB" + version byte
```

The current format version is:

```text
2
```

Each record is encoded as:

```text
[type:1][payloadLength:4][checksum:4][payload:N]
```

All 32-bit integer fields are stored little-endian.

### PUT payload

```text
[keyLength:4][key][valueLength:4][value]
```

### DELETE payload

```text
[keyLength:4][key]
```

Maximum sizes:

```text
Key:    1 MiB
Value: 64 MiB
```

## Record Integrity

Each record contains a CRC32/IEEE checksum.

Parameters:

```text
Polynomial:  0xEDB88320
Initial:     0xFFFFFFFF
Final XOR:   0xFFFFFFFF
```

The checksum covers:

```text
type + encoded payloadLength + payload
```

The checksum field itself is excluded.

Complete records with invalid checksums or malformed data are treated as database corruption.

## Startup Replay and Recovery

On startup, KeyServe validates the database header and streams records from disk one at a time.

It does not load the entire log into a temporary record collection.

A clean end-of-file is normal.

If the final record is incomplete, KeyServe treats it as an interrupted append:

1. The start offset of the incomplete record is remembered.
2. The database is truncated back to that offset.
3. The repaired file is synchronized to disk.
4. Replay stops at the last valid record.

Only an incomplete final record is automatically repaired.

The following remain fatal:

- invalid record types
- malformed complete records
- impossible payload sizes
- CRC mismatches
- invalid database headers
- unsupported database versions

## Compaction

Because the database is append-only, overwritten values and DELETE records eventually become obsolete.

Compaction rewrites the current logical database into a new file containing only live PUT records.

The active database is left untouched until the replacement database has been completely written and synchronized.

The compaction sequence is approximately:

```text
create temporary database
        ↓
copy permissions
        ↓
write database header
        ↓
write current live PUT records
        ↓
fsync temporary database
        ↓
prepare future read/write descriptors
        ↓
open parent directory
        ↓
rename temporary DB over active DB
        ↓
fsync parent directory
        ↓
switch persistent descriptors
```

The `rename()` is the commit point.

Before a successful rename, a storage failure leaves the original database untouched and compaction can safely fail.

After the rename succeeds, failures are treated as fatal because the persistent database pathname has already been replaced.

## Automatic Compaction

KeyServe tracks two sizes incrementally:

```text
currentSize
```

The physical size of the append-only database file.

```text
liveAfterCompact
```

The expected physical size of the database if it were compacted immediately.

The wasted space is:

```text
currentSize - liveAfterCompact
```

Automatic compaction currently occurs when both conditions are met:

```text
wasted space >= 1 MiB
```

and:

```text
wasted space / current size >= 30%
```

The counters include the five-byte database header.

Size accounting is reconstructed when the database starts and then updated incrementally after successful writes.

Automatic compaction is currently synchronous. Background compaction is intentionally deferred until the project introduces a proper concurrency model.

## Error Model

KeyServe uses the following exception hierarchy:

```text
KeyServeError
├── StorageError
└── CorruptionError
    └── IncompleteRecordError
```

### `StorageError`

Represents operating-system or file-access failures.

### `CorruptionError`

Represents database bytes that violate the expected file format.

### `IncompleteRecordError`

Represents a truncated final record that can be safely removed during startup recovery.

## Crash-Safety Principles

The current storage engine follows several important rules:

- Persist mutations before changing the in-memory database.
- Retry interrupted POSIX reads and writes.
- Handle partial reads and writes explicitly.
- Synchronize newly created database files before using them.
- Repair only incomplete final records.
- Never silently repair complete corrupted records.
- Never replace the active database until a complete compacted database has been written and synced.
- Synchronize the parent directory after atomic replacement.

## Project Structure

```text
server-db/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── include/
│   ├── database.hpp
│   ├── storage.hpp
│   ├── errors.hpp
│   └── UniqueFd.hpp
└── src/
    ├── main.cpp
    ├── database.cpp
    ├── storage.cpp
    └── UniqueFd.cpp
```

## Current Architecture

`Database` owns the logical key-value state and exposes the database operations.

`Storage` owns persistence concerns including:

- file creation and validation
- serialization and parsing
- appending records
- streaming replay
- synchronization
- incomplete-tail recovery
- size accounting
- compaction

`UniqueFd` provides RAII ownership for POSIX file descriptors.

## Roadmap

The next major storage milestone is moving from:

```cpp
std::unordered_map<std::string, std::string>
```

toward an in-memory index containing locations of records stored on disk.

This will allow values to remain on disk and make `GET` perform offset-based reads, likely using `pread()`.

Later milestones include:

- disk-backed values with an in-memory offset index
- codec/serialization extraction
- page/block-based storage
- reusable/free disk pages
- TCP client/server support
- authentication
- concurrent clients
- worker threads
- background maintenance/compaction
- configurable database paths and storage policies
- optional custom hash-map implementation

## Project Purpose

KeyServe is primarily a learning project.

The goal is not only to produce a working key-value database, but to progressively explore the systems concepts behind one:

- binary file formats
- persistence
- crash consistency
- POSIX I/O
- data integrity
- recovery
- indexing
- disk layout
- networking
- concurrency
- synchronization