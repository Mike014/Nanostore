# NanoStore

**Embedded, thread-safe, crash-resilient key-value storage engine — C++23**

Built from scratch as a hands-on implementation of the storage engine concepts described in *Designing Data-Intensive Applications* (Kleppmann, Ch. 3 and Ch. 7). Same category as LevelDB and RocksDB — no server, no network, no dependencies.

---

## What it is

NanoStore is an **embedded** key-value store. It runs inside the same process as the application that uses it — no separate server, no socket, no network round-trip. You link it, you call it, it works.

The data model is simple: every value is addressed by a string key. Under the hood, the engine maintains an in-memory hash index (`std::unordered_map`) backed by a persistent Write-Ahead Log on disk. Reads are served from memory with no I/O. Writes go to disk first, then to memory.

---

## What is the WAL

The **Write-Ahead Log** is the mechanism that makes data survive a crash.

Without a WAL, every write goes directly to RAM. RAM is volatile — if the process dies, the data is gone.

```
WITHOUT WAL:
set("user:1", "Michele")  →  RAM only
CRASH
→ data lost
```

With a WAL, every write is first appended to a binary file on disk, then applied to the in-memory index. On restart, the engine replays the log from the beginning and reconstructs the index.

```
WITH WAL:
set("user:1", "Michele")  →  disk (WAL)  →  RAM
CRASH
→ restart: replay WAL  →  RAM reconstructed  →  data intact
```

The WAL is append-only — every operation adds a record at the end, nothing is ever overwritten. This makes writes sequential and fast. The tradeoff is that the file grows indefinitely, which is why compaction exists.

**Compaction** solves the growth problem: after 10,000 operations, the engine writes the current state of the index to a `.tmp` file, then atomically replaces the old WAL with it. The log starts fresh. On Windows this uses `ReplaceFileW` (Win32 API) for a true atomic replace. On POSIX it uses `rename()`.

---

## Features

- Write-Ahead Log with binary format and CRC32 integrity check
- Crash recovery via full WAL replay on startup
- Automatic compaction after 10,000 operations (atomic rename)
- Thread-safe: `std::shared_mutex` — shared reads, exclusive writes
- Custom exception hierarchy: `WalCorruptionException`, `KeyNotFoundException`, `StorageIOException`
- 22 test cases, 1537 assertions — Catch2 v3

---

## Quick Start

```sh
git clone https://github.com/<your-username>/nanostore.git
cd nanostore
make
./nanostore
```

Requires g++ 14+ with C++23 support (MinGW or GCC).

---

## API

```cpp
StorageEngine engine("data/wal.log");

// Write
engine.set("user:1", "Michele");
engine.del("user:1");
engine.clear();

// Read
std::optional<std::string> val = engine.get("user:1");
std::string val = engine.get_or_throw("user:1");   // throws KeyNotFoundException if missing

// Query
bool found        = engine.exists("user:1");
size_t n          = engine.count();
auto keys         = engine.scan("user:");           // all keys with this prefix
```

| Method            | Lock          | Description                                        |
|-------------------|---------------|----------------------------------------------------|
| `set(k, v)`       | exclusive     | Insert or update                                   |
| `get(k)`          | shared        | Returns `std::optional<std::string>`               |
| `get_or_throw(k)` | shared        | Returns value or throws `KeyNotFoundException`     |
| `del(k)`          | exclusive     | Remove key                                         |
| `exists(k)`       | shared        | Returns bool                                       |
| `count()`         | shared        | Number of keys in index                            |
| `clear()`         | exclusive     | Remove all keys and compact WAL                    |
| `scan(prefix)`    | shared        | Returns all keys starting with prefix              |

---

## Architecture

```
caller
  │
  ▼
StorageEngine
  ├── unique_lock (writes)
  │     ├── WAL::append()  →  disk (binary log)
  │     └── index_[key] = value  →  RAM
  │
  ├── shared_lock (reads)
  │     └── index_.find(key)  →  RAM only, no I/O
  │
  └── on startup
        └── WAL::recover()  →  replay log  →  rebuild index_
```

**Write path:** lock → WAL append → index update → compaction check

**Read path:** lock → index lookup (no disk I/O)

**Recovery path:** open WAL → validate magic/version header → read entries → validate CRC32 → replay into index

---

## WAL Binary Format

Each entry on disk:

```
┌──────────┬──────────┬──────────────┬──────────────┬────────────┐
│  op (1)  │ klen (4) │  key (klen)  │  vlen (4)    │ val (vlen) │
└──────────┴──────────┴──────────────┴──────────────┴────────────┘
└─────────────────────────────────────────────────── crc32 (4) ──┘
```

File header (5 bytes, written during compaction):

```
┌──────────────────────────┬─────────────┐
│  magic: "NSWA" (4 bytes) │ version (1) │
└──────────────────────────┴─────────────┘
```

- `op`: `0x00` = SET, `0x01` = DEL
- `klen` / `vlen`: 32-bit unsigned, native endian (little-endian on x86/x64)
- `crc32`: PKZIP reflected polynomial, computed over `op + klen + key + vlen + value`
- Max key size: 1 MB. Max value size: 100 MB. Exceeded limits throw `StorageIOException`.

---

## Testing

```sh
make test       # 22 test cases, 1537 assertions
make test-san   # same with -fsanitize=address,undefined
```

| Category    | Scenarios                                              |
|-------------|--------------------------------------------------------|
| Core ops    | set, get, del, overwrite                               |
| Edge cases  | empty key, empty value, del nonexistent, clear empty   |
| Persistence | WAL recovery, compaction + recovery after compaction   |
| Corruption  | CRC mismatch, truncated WAL, invalid op type           |
| Concurrency | 4 writer threads + 2 reader threads simultaneously     |
| API         | exists, count, clear, scan, get_or_throw               |

---

## Design Decisions

**Why WAL before index update?**
If the process crashes after updating RAM but before writing disk, the data is lost. Writing to the WAL first guarantees durability — on restart the log is replayed and the index is rebuilt. This is the D in ACID.

**Why CRC32 and not SHA-1 or xxHash?**
CRC32 detects accidental corruption — bit rot, partial writes, truncated entries. It is not cryptographic, but that is not the goal. The lookup table implementation is portable and fast with no external dependencies.

**Why `ReplaceFileW` on Windows instead of `rename`?**
`std::rename` on Windows fails if the destination already exists. The naive fix — `remove` then `rename` — creates a window where both files are gone. `ReplaceFileW` is the Win32 API that provides a true atomic replacement with an optional backup file.

**Why `peek()` + `gcount()` instead of `failbit`?**
`std::ifstream` on MinGW sets `failbit` on partial binary reads. Using `peek() != EOF` and `gcount()` checks avoids clearing stream flags after every read and correctly detects truncated entries.

**Why shared_mutex instead of mutex?**
A plain `std::mutex` serializes all operations including concurrent reads. `std::shared_mutex` allows any number of readers to proceed simultaneously — only writers need exclusive access. This is the standard pattern for read-heavy workloads.

---

## Project Structure

```
nanostore/
├── src/
│   ├── storage_engine.hpp   # public API
│   ├── storage_engine.cpp   # thread-safe engine logic
│   ├── wal.hpp              # WAL class + entry format
│   ├── wal.cpp              # binary I/O, CRC32, compaction
│   ├── exceptions.hpp       # exception hierarchy
│   └── main.cpp             # demo entry point
├── tests/
│   ├── test_storage.cpp     # Catch2 test suite
│   ├── catch_amalgamated.hpp
│   └── catch_amalgamated.cpp
├── data/                    # WAL files — gitignored
├── Makefile
├── .gitignore
└── README.md
```

---

## Concept

This project is a direct implementation of concepts from *Designing Data-Intensive Applications* by Martin Kleppmann:

- **Ch. 3 — Storage and Retrieval**: hash indexes, log-structured storage, compaction as write amplification control
- **Ch. 7 — Transactions**: durability guarantees, crash recovery, the role of the WAL in making writes atomic from the caller's perspective

Practical lessons:

- Binary file I/O portability pitfalls between POSIX and Windows/MinGW
- CRC32 from first principles — lookup table, polynomial arithmetic, why XOR of CRCs is not a CRC of concatenated data
- The `rename` atomicity problem on Windows and how `ReplaceFileW` solves it
- `std::shared_mutex` patterns for read-optimized concurrent workloads
- WAL recovery design: full replay, corruption detection, truncation tolerance

---

## Roadmap

Planned features — see `PRODUCTION_ROADMAP.md`:

- Snapshot support: periodic full-index dump to reduce recovery time on large WALs
- TTL: `setex(key, value, ttl_ms)` with lazy expiration
- Networking: RESP protocol (Redis-compatible) or gRPC
- CMake: replace Makefile for cross-platform builds and test discovery