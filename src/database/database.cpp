#include "database.hpp"

#include "database_structs.hpp"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>

Database::Database(const std::string& filename)
    : filename(filename)
{
}


bool Database::initialize()
{
    std::filesystem::create_directories("data");

    std::ifstream in(filename, std::ios::binary);

    if (in.good())
    {
        in.close();
        return true;
    }

    in.close();

    std::ofstream out(filename, std::ios::binary);

    if (!out)
        return false;

    DatabaseHeader header;

    out.write(reinterpret_cast<const char*>(&header),
              sizeof(DatabaseHeader));
    
    std::cout << "Database initialized successfully.\n";
std::cout << "Header size: " << sizeof(DatabaseHeader) << " bytes\n";

    out.close();

    return true;
}

std::string Database::execute(const std::string& command)
{
    if (command == "help")
    {
        return
            "Available commands:\n"
            "  help\n"
            "  create collection <name>\n"
            "  drop collection <name>\n"
            "  list collections\n"
            "  insert <collection>\n"
            "  find <collection>\n"
            "  update <collection>\n"
            "  delete <collection>\n";
    }

    return "Unknown command.";
}

bool Database::createCollection(const std::string&)
{
    return false;
}

bool Database::dropCollection(const std::string&)
{
    return false;
}

bool Database::collectionExists(const std::string&)
{
    return false;
}

std::vector<std::string> Database::listCollections()
{
    return {};
}

bool Database::insertDocument(const std::string&,
                              const std::string&)
{
    return false;
}

std::vector<std::string> Database::findDocuments(
    const std::string&)
{
    return {};
}

bool Database::updateDocument(const std::string&,
                              const std::string&,
                              const std::string&)
{
    return false;
}

bool Database::deleteDocument(const std::string&,
                              const std::string&)
{
    return false;
}
