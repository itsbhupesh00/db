#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <string>
#include <vector>

class Database
{
public:
    explicit Database(const std::string& filename);

    bool initialize();

    std::string execute(const std::string& command);

private:
    bool createCollection(const std::string& name);
    bool dropCollection(const std::string& name);
    bool collectionExists(const std::string& name);

    std::vector<std::string> listCollections();

    bool insertDocument(const std::string& collection,
                        const std::string& json);

    std::vector<std::string> findDocuments(const std::string& collection);

    bool updateDocument(const std::string& collection,
                        const std::string& condition,
                        const std::string& json);

    bool deleteDocument(const std::string& collection,
                        const std::string& condition);

private:
    std::string filename;
};

#endif
