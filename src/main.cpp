#include <iostream>

#include "database.hpp"

int main()
{
    Database db;

    db.put("duck", "yellow");
    db.put("number", "42");

    auto value = db.get("duck");

    if (value.has_value())
    {
        std::cout << "duck = " << value.value() << '\n';
    }

    db.put("duck", "green");

    value = db.get("duck");

    if (value.has_value())
    {
        std::cout << "duck = " << value.value() << '\n';
    }

    std::cout << "number exists: "
              << db.exists("number")
              << '\n';

    std::cout << "removed number: "
              << db.remove("number")
              << '\n';

    std::cout << "number exists: "
              << db.exists("number")
              << '\n';

    return 0;
}