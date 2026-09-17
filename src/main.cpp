#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#include "database.hpp"
#include "errors.hpp"
#include "storage.hpp"

enum COMMAND { PUT, DELETE, GET, EXISTS, EXIT, UNKNOWN };

static COMMAND resolveCommand(const std::string& input) {
    static const std::unordered_map<std::string, COMMAND> commandMap = {
        {"PUT", PUT},
        {"DELETE", DELETE},
        {"GET", GET},
        {"EXISTS", EXISTS},
        {"EXIT", EXIT}};

    auto it = commandMap.find(input);
    if (it != commandMap.end()) {
        return it->second;
    }
    return UNKNOWN;
}

static void handleGET(Database& db, std::istringstream& input) {
    std::string key;

    if (!(input >> key)) {
        std::cout << "Usage: GET <key>\n";
        return;
    }

    auto value = db.get(key);

    if (value.has_value()) {
        std::cout << key << " = " << value.value() << std::endl;
    } else {
        std::cout << "NOT_FOUND\n";
    }
}

static void handleExists(Database& db, std::istringstream& input) {
    std::string key;

    if (!(input >> key)) {
        std::cout << "Usage: EXISTS <key>\n";
        return;
    }

    std::cout << ((db.exists(key)) ? "true" : "false") << std::endl;
}

static void handleDELETE(Database& db, std::istringstream& input) {
    std::string key;

    if (!(input >> key)) {
        std::cout << "Usage: DELETE <key>\n";
        return;
    }

    if (db.remove(key)) {
        std::cout << "OK\n";
    } else {
        std::cout << "NOT_FOUND\n";
    }
}

static void handlePUT(Database& db, std::istringstream& input) {
    std::string key;
    if (!(input >> key)) {
        std::cout << "Usage: PUT <key> <value>\n";
        return;
    }

    std::string value;
    std::getline(input >> std::ws, value);

    db.put(key, value);
    std::cout << "OK\n";
}

// handles command calling. returns false if command if need to exit keyserve
static bool handleCommands(Database& db) {
    std::string line;

    std::cout << "KeyServe> ";

    if (!std::getline(std::cin, line)) {
        return false;
    }

    std::istringstream input(line);

    std::string command;
    input >> command;

    if (command.empty()) {
        return true;
    }

    switch (resolveCommand(command)) {
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
            return false;
        default:
            std::cout << "Unknown command: " << command << std::endl;
    }

    return true;
}

int main() {
    try {
        Database db("keyserve.db");

        std::cout << "KeyServe\n";
        std::cout << "Type EXIT to quit.\n";

        while (handleCommands(db));
    } catch (const CorruptionError& error) {
        std::cerr << "FATAL: database corruption: " << error.what() << '\n';
        return 1;
    } catch (const StorageError& error) {
        std::cerr << "FATAL: storage error: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "FATAL: unexpected error: " << error.what() << '\n';
        return 1;
    }
}