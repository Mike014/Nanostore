#include "catch_amalgamated.hpp"
#include "../src/storage_engine.hpp"
#include "../src/exceptions.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <fstream>

int main(int argc, char *argv[])
{
    return Catch::Session().run(argc, argv);
}

TEST_CASE("set e get base")
{
    StorageEngine engine("data/test.log");
    engine.set("user:1", "Michele");
    REQUIRE(engine.get("user:1").value() == "Michele");
    REQUIRE_FALSE(engine.get("nonexistent").has_value());
}

TEST_CASE("del rimuove la chiave")
{
    StorageEngine engine("data/test_del.log");

    engine.set("user:1", "Michele");
    engine.del("user:1");

    REQUIRE_FALSE(engine.get("user:1").has_value());
}

TEST_CASE("sovrascrittura")
{
    StorageEngine engine("data/test_rewrite.log");

    engine.set("user:1", "Michele");
    engine.set("user:1", "Luca");

    REQUIRE(engine.get("user:1").value() == "Luca");
}

TEST_CASE("WAL vuoto")
{
    StorageEngine engine("data/test_empty.log");

    REQUIRE_FALSE(engine.get("user:1").has_value());
}

TEST_CASE("recovery da WAL al riavvio")
{
    {
        StorageEngine engine("data/test_recovery.log");
        engine.set("user:1", "Michele");
        engine.set("user:2", "Luca");
        engine.del("user:2");
    }

    StorageEngine engine2("data/test_recovery.log");
    REQUIRE(engine2.get("user:1").value() == "Michele");
    REQUIRE_FALSE(engine2.get("user:2").has_value());
}

TEST_CASE("stress test 1000 operazioni")
{
    {
        StorageEngine engine("data/test_stress.log");
        for (int i = 0; i < 1000; i++)
        {
            engine.set("key:" + std::to_string(i), "val:" + std::to_string(i));
        }
    }

    StorageEngine engine2("data/test_stress.log");
    for (int i = 0; i < 1000; i++)
    {
        std::string key = "key:" + std::to_string(i);
        std::string expected = "val:" + std::to_string(i);
        REQUIRE(engine2.get(key).value() == expected);
    }
}

TEST_CASE("concorrenza — scrittura e lettura simultanea")
{
    StorageEngine engine("data/test_concurrency.log");

    std::vector<std::thread> writers;
    for (int t = 0; t < 4; t++)
    {
        writers.emplace_back([&engine, t]()
                             {
            for (int i = 0; i < 100; i++)
            {
                std::string key = "t" + std::to_string(t) + ":key:" + std::to_string(i);
                std::string value = "val:" + std::to_string(i);
                engine.set(key, value);
            } });
    }

    std::atomic<bool> stop{false};
    std::vector<std::thread> readers;
    for (int t = 0; t < 2; t++)
    {
        readers.emplace_back([&engine, &stop]()
                             {
            while (!stop)
            {
                engine.get("t0:key:0");
                engine.get("t1:key:50");
            } });
    }

    for (auto &w : writers)
        w.join();
    stop = true;
    for (auto &r : readers)
        r.join();

    for (int t = 0; t < 4; t++)
    {
        for (int i = 0; i < 100; i++)
        {
            std::string key = "t" + std::to_string(t) + ":key:" + std::to_string(i);
            std::string expected = "val:" + std::to_string(i);
            REQUIRE(engine.get(key).value() == expected);
        }
    }
}

TEST_CASE("exists")
{
    StorageEngine engine("data/test_exists.log");
    engine.set("user:1", "Michele");

    REQUIRE(engine.exists("user:1") == true);
    REQUIRE(engine.exists("user:99") == false);

    engine.del("user:1");
    REQUIRE(engine.exists("user:1") == false);
}

TEST_CASE("count")
{
    StorageEngine engine("data/test_count.log");

    REQUIRE(engine.count() == 0);

    engine.set("user:1", "Michele");
    engine.set("user:2", "Luca");
    REQUIRE(engine.count() == 2);

    engine.del("user:1");
    REQUIRE(engine.count() == 1);
}

TEST_CASE("clear")
{
    StorageEngine engine("data/test_clear.log");

    engine.set("user:1", "Michele");
    engine.set("user:2", "Luca");
    REQUIRE(engine.count() == 2);

    engine.clear();
    REQUIRE(engine.count() == 0);
    REQUIRE(engine.exists("user:1") == false);
}

TEST_CASE("scan per prefisso")
{
    StorageEngine engine("data/test_scan.log");

    engine.set("user:1", "Michele");
    engine.set("user:2", "Luca");
    engine.set("order:1", "Pizza");

    auto results = engine.scan("user:");
    REQUIRE(results.size() == 2);

    auto orders = engine.scan("order:");
    REQUIRE(orders.size() == 1);

    auto empty = engine.scan("nonexistent:");
    REQUIRE(empty.size() == 0);
}

TEST_CASE("get_or_throw — chiave esistente")
{
    StorageEngine engine("data/test_got.log");
    engine.set("user:1", "Michele");

    REQUIRE_NOTHROW(engine.get_or_throw("user:1"));
    REQUIRE(engine.get_or_throw("user:1") == "Michele");
}

TEST_CASE("get_or_throw — chiave inesistente lancia KeyNotFoundException")
{
    StorageEngine engine("data/test_got_missing.log");

    REQUIRE_THROWS_AS(engine.get_or_throw("nonexistent"), KeyNotFoundException);
    REQUIRE_THROWS_AS(engine.get_or_throw("nonexistent"), NanoStoreException);
    REQUIRE_THROWS_AS(engine.get_or_throw("nonexistent"), std::exception);
}

TEST_CASE("WAL corrotto — WalCorruptionException")
{
    const char *path = "data/test_corrupt.log";

    {
        StorageEngine engine(path);
        engine.set("user:1", "Michele");
    }

    // Compute offset into the key region: header(5) + op(1) + klen(4) = 10
    constexpr int wal_header_size = 5;
    std::string key = "user:1";
    std::streamoff corrupt_offset = wal_header_size + 1 + 4 + key.size() - 1;

    {
        std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
        REQUIRE(f.is_open());
        f.seekp(corrupt_offset, std::ios::beg);
        char bad = 0xFF;
        f.write(&bad, 1);
    }

    REQUIRE_THROWS_AS(StorageEngine(path), WalCorruptionException);
    REQUIRE_THROWS_AS(StorageEngine(path), NanoStoreException);
}

TEST_CASE("WAL inesistente — StorageIOException")
{
    REQUIRE_THROWS_AS(StorageEngine("data/\0invalid.log"), StorageIOException);
}

TEST_CASE("get_or_throw — catch come std::exception")
{
    StorageEngine engine("data/test_got_catch.log");

    try
    {
        engine.get_or_throw("nonexistent");
        FAIL("should have thrown");
    }
    catch (const std::exception &e)
    {
        std::string msg = e.what();
        REQUIRE(msg.find("key not found") != std::string::npos);
    }
}

TEST_CASE("empty key")
{
    StorageEngine engine("data/test_empty_key.log");
    engine.set("", "empty_key_value");
    REQUIRE(engine.get("").value() == "empty_key_value");
}

TEST_CASE("empty value")
{
    StorageEngine engine("data/test_empty_value.log");
    engine.set("empty_value_key", "");
    REQUIRE(engine.get("empty_value_key").value() == "");
}

TEST_CASE("del on non-existent key")
{
    StorageEngine engine("data/test_del_nonexist.log");
    REQUIRE_NOTHROW(engine.del("does_not_exist"));
}

TEST_CASE("clear on empty store")
{
    StorageEngine engine("data/test_clear_empty.log");
    REQUIRE_NOTHROW(engine.clear());
    REQUIRE(engine.count() == 0);
}

TEST_CASE("compaction trigger and recovery after compaction")
{
    const char *path = "data/test_compaction.log";

    {
        StorageEngine engine(path);
        // Force >10000 operations to trigger compaction
        for (int i = 0; i < 10000; i++)
        {
            engine.set("k:" + std::to_string(i), "v:" + std::to_string(i));
        }
        engine.set("final", "check");
    }

    // Recover from the compacted WAL
    StorageEngine engine2(path);
    REQUIRE(engine2.get("final").value() == "check");
    // Verify random entries survived
    for (int i = 0; i < 100; i++)
    {
        int idx = i * 100;
        REQUIRE(engine2.get("k:" + std::to_string(idx)).value() == "v:" + std::to_string(idx));
    }
}

TEST_CASE("recovery from truncated WAL")
{
    const char *path = "data/test_truncated.log";

    // Write one valid entry
    {
        StorageEngine engine(path);
        engine.set("survivor", "data");
    }

    // Append a partial entry at the end to simulate a crash
    {
        std::ofstream f(path, std::ios::app | std::ios::binary);
        REQUIRE(f.is_open());
        uint8_t partial[] = { 0x00, 0x01, 0x00, 0x00, 0x00 }; // op=SET, klen=1 but no key
        f.write(reinterpret_cast<const char*>(partial), sizeof(partial));
    }

    // Recovery should succeed, ignoring the truncated entry
    StorageEngine engine(path);
    REQUIRE(engine.get("survivor").value() == "data");
}
