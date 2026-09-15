# KeyServe

KeyServe is a learning project to build a persistent key-value database and network database server from scratch in C++.

The project starts as a simple local key-value store and will gradually grow into a server that can accept authenticated requests from clients over a network. The goal is to understand how storage engines, persistence, network protocols, authentication, concurrency, and Linux services work by implementing the important parts myself.

## Planned Architecture

```text
Client
  |
  | TCP
  v
KeyServe Server
  |
  v
Database
  |
  +-- In-memory index
  |
  +-- Persistent storage
          |
          v
      database file
```

Development will begin locally under WSL/Linux. Once the database and server are working, the server will be deployed to a separate Linux machine and accessed by clients over a LAN.

## Planned Features

- [ ] Basic in-memory key-value database
- [ ] Persistent on-disk storage
- [ ] Disk-backed values with an in-memory index
- [ ] Updates and deletions
- [ ] Database compaction
- [ ] Recovery and corruption handling
- [ ] TCP client/server protocol
- [ ] Access over a local network
- [ ] Client authentication
- [ ] Multiple concurrent clients
- [ ] Linux service deployment

Possible later extensions include transactions, a write-ahead log, B+ tree or LSM-style indexing, caching, and improved network/security features.

## Database Interface

The initial database will support a deliberately small set of operations:

```text
PUT key value
GET key
DELETE key
EXISTS key
```

Keys and values initially consist of arbitrary strings. More complex types are intentionally being left out until the storage engine works.

## Purpose

This is primarily a learning project rather than an attempt to build a production database.

The aim is to explore the implementation details normally hidden behind existing database and networking libraries, including:

- binary serialization and file formats
- indexing and random file access
- crash consistency
- TCP sockets and protocol framing
- authentication
- synchronization and concurrent access
- Linux deployment and service management

## Status

🚧 Work in progress — currently setting up the project and implementing the first database interface.
