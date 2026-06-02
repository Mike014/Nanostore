#include <iostream>
#include "exceptions.hpp"
#include "storage_engine.hpp"

int main()
{
    try
    {
        StorageEngine engine("data/wal.log");

        engine.set("user:1", "Michele");
        engine.set("user:2", "Luca");
        engine.del("user:1");

        std::cout << engine.get("user:1").value_or("NOT FOUND") << "\n";
        std::cout << engine.get("user:2").value_or("NOT FOUND") << "\n";

        try
        {
            std::string val = engine.get_or_throw("user:1");
            std::cout << "get_or_throw(user:1) = " << val << "\n";
        }
        catch (const KeyNotFoundException &e)
        {
            std::cout << "Caught KeyNotFoundException: " << e.what() << "\n";
        }

        std::cout << "Done.\n";
    }
    catch (const NanoStoreException &e)
    {
        std::cerr << "NanoStore error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return 1;
    }
}
