# NanoStore — Production Roadmap

## Current Status

Key-value engine with binary WAL, CRC32, compaction, thread-safe concurrency, and complete test suite on MinGW.

---

## Done

### 1. Bug fix — Hardcoded path in `WAL::recover()`

`recover()` now uses `path_` (class member) instead of the hardcoded `"data/wal.log"`.

### 2. WAL — Binary format + CRC32

Entry format:

```
[1 byte op] [4 byte key_len] [key] [4 byte val_len] [value] [4 byte crc32]
```

- CRC32 via lookup table for corruption detection
- CRC computed over a contiguous buffer (correct — not XOR of separate CRCs)
- Sanity checks on key_len (max 1 MB) and val_len (max 100 MB)
- Robust reading with `peek() + gcount()` instead of `failbit` (works on Windows/MinGW)

### 3. WAL — Compaction

- `compact(const unordered_map& index)` method in `WAL`
- Writes current state to `.tmp` file, then atomic `rename()`
- Triggered after 10,000 operations (`MAX_OPS_BEFORE_COMPACT`)
- On Windows: uses `ReplaceFileW` (Win32 API) for true atomic replacement
- On POSIX: uses `rename()` directly

### 4. Test setup — Catch2 v3 amalgamated + MinGW

- Tests running with Catch2 v3.5.3 amalgamated
- Custom main with `CATCH_AMALGAMATED_CUSTOM_MAIN`
- Static linking (`-static`) to avoid `0xc0000139` on missing DLLs
- Makefile with `make test` and `make test-san` targets

### 5. Tests — Basic scenarios

- `set` + `get` base case
- `del` + `get` → nullopt
- Key overwrite
- Empty / nonexistent WAL on startup

### 6. Tests — Advanced scenarios

- WAL recovery on restart (set + del survive object destruction)
- Stress test: 1000 operations, verify final state after recovery
- Compaction trigger: force >10,000 ops, verify WAL file size shrank
- Recovery after compaction: set data, trigger compaction, destroy engine, reconstruct, verify data intact
- Edge cases: empty key, empty value, del on nonexistent key, clear on empty store
- Corruption: CRC mismatch, truncated WAL, invalid op type

### 7. Concurrency — `std::shared_mutex`

- `get()` → `shared_lock` (multiple simultaneous readers)
- `set()` / `del()` → `unique_lock` (exclusive writer)
- Test: 4 writer threads + 2 reader threads simultaneously
- 22 test cases, 1537 assertions — all passed

### 8. Extended public API

- `bool exists(const std::string& key)`
- `std::vector<std::string> scan(const std::string& prefix)`
- `size_t count()`
- `void clear()`
- `get_or_throw(key)` — throws `KeyNotFoundException` if missing

### 9. Error handling

Custom exception hierarchy:

```
std::runtime_error
  └── NanoStoreException
        ├── WalCorruptionException
        ├── KeyNotFoundException
        └── StorageIOException
```

### 10. Code review — All 13 items fixed

- Atomic compaction via `ReplaceFileW` on Windows
- `file_.is_open()` guard in `append()`
- `op_count_` reset in `clear()`
- `#include <cstdint>` added to `wal.hpp`
- Size bounds in `write_entry()` matching `recover()` limits
- `OperationType` validation during recovery
- WAL magic/version header (`NSWA` + `0x01`)
- Explicit `= delete` for copy/move on `StorageEngine`
- `[[nodiscard]]` on all query methods
- Corruption test offset computed from constants
- Stale files and unused includes removed
- `.gitignore` and sanitizer Makefile target added

### 11. README

- Project description and architecture
- WAL concept explanation (with/without WAL comparison)
- WAL binary format diagram
- Full API reference table
- Design decisions (WAL, CRC32, atomic rename, shared_mutex)
- What I Learned — references to DDIA Ch. 3 and Ch. 7
- Build and test instructions
- Roadmap section

---

## To Do

### 12. Periodic snapshots

In addition to the WAL, save the entire `unordered_map` to a snapshot file. On recovery: load snapshot + replay only the WAL written after it. Reduces boot time on large datasets.

### 13. CMake

Replace Makefile with CMake for cross-platform builds and automatic test discovery.

```cmake
cmake_minimum_required(VERSION 3.20)
project(nanostore)
add_executable(nanostore src/main.cpp src/wal.cpp src/storage_engine.cpp)
```

### 14. Networking (optional)

- **RESP** (Redis protocol) — compatible with Redis clients
- **gRPC** — multi-language
- **HTTP (crow/drogon)** — REST API

### 15. Configuration (optional)

JSON/TOML config file:

```json
{
  "wal_path": "data/wal.log",
  "wal_max_size_mb": 64,
  "snapshot_interval_sec": 300,
  "sync_mode": "async",
  "bind": "127.0.0.1:6379"
}
```

### 16. Bulk operations

- `mset(pairs)` — batch insert
- `mget(keys)` — batch read
- `mdel(keys)` — batch delete

### 17. TTL support

- `setex(key, value, ttl_ms)` — key with expiration
- Lazy expiration on read + periodic background cleanup

---

## Updated Priority

1. ~~Fix bug `recover()`~~ ✅
2. ~~Robust WAL (binary + checksum + compaction)~~ ✅
3. ~~Test setup (Catch2 + MinGW)~~ ✅
4. ~~Basic test scenarios~~ ✅
5. ~~Advanced test scenarios~~ ✅
6. ~~Concurrency (`shared_mutex`)~~ ✅
7. ~~Extended API (exists, scan, count, clear, get_or_throw)~~ ✅
8. ~~Error handling (exception hierarchy)~~ ✅
9. ~~Code review — all 13 fixes applied~~ ✅
10. ~~README~~ ✅
11. **Snapshot support**
12. CMake
13. Networking (optional)
14. Configuration (optional)
15. Bulk operations
16. TTL support