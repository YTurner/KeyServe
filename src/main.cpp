#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#include "database.hpp"

enum COMMAND
{
    PUT,
    DELETE,
    GET,
    EXISTS,
    EXIT,
    UNKNOWN
};

static COMMAND resolveCommand(const std::string &input)
{
    static const std::unordered_map<std::string, COMMAND> commandMap = {
        {"PUT", PUT},
        {"DELETE", DELETE},
        {"GET", GET},
        {"EXISTS", EXISTS},
        {"EXIT", EXIT}};

    auto it = commandMap.find(input);
    if (it != commandMap.end())
    {
        return it->second;
    }
    return UNKNOWN;
}

static void handleGET(Database &db, std::istringstream &input)
{
    std::string key;

    if (!(input >> key))
    {
        std::cout << "Usage: GET <key>\n";
        return;
    }

    auto value = db.get(key);

    if (value.has_value())
    {
        std::cout << key << " = " << value.value() << std::endl;
    }
    else
    {
        std::cout << "NOT_FOUND\n";
    }
}

static void handleExists(Database &db, std::istringstream &input)
{
    std::string key;

    if (!(input >> key))
    {
        std::cout << "Usage: EXISTS <key>\n";
        return;
    }

    std::cout << ((db.exists(key))? "true" : "false") << std::endl;
}

static void handleDELETE(Database &db, std::istringstream &input)
{
    std::string key;

    if (!(input >> key))
    {
        std::cout << "Usage: DELETE <key>\n";
        return;
    }

    if (db.remove(key))
    {
        std::cout << "OK\n";
    }
    else
    {
        std::cout << "NOT_FOUND\n";
    }
}

static void handlePUT(Database &db, std::istringstream &input)
{
    std::string key;
    if (!(input >> key))
    {
        std::cout << "Usage: PUT <key> <value>\n";
        return;
    }

    std::string value;
    std::getline(input >> std::ws, value);

    db.put(key, value);
    std::cout << "OK\n";
}

int main()
{
    Database db;
    std::string line;

    std::cout << "KeyServe\n";
    std::cout << "Type EXIT to quit.\n";

    while (true)
    {
        std::cout << "KeyServe> ";

        if (!std::getline(std::cin, line))
        {
            break;
        }

        std::istringstream input(line);

        std::string command;
        input >> command;

        if (command.empty())
        {
            continue;
        }

        switch (resolveCommand(command))
        {
        case PUT:
            handlePUT(db, input);
            break;
        case DELETE:
            handleDELETE(db, input);
            break;
        case GET:
            handleGET(db, input);
            break;
        case EXISTS:
            handleExists(db, input);
            break;
        case EXIT:
            return 0;
        default:
            std::cout << "Unknown command: " << command << std::endl;
        }
    }

    return 0;
}