#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <cstdint>
#include "database_structs.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

enum class ValueType : uint8_t
{
    Null = 0,
    Boolean = 1,
    Integer = 2,
    Double = 3,
    String = 4
};

struct Value
{
    ValueType type = ValueType::Null;
    bool booleanValue = false;
    int64_t integerValue = 0;
    double doubleValue = 0.0;
    std::string stringValue;
};

using DocumentFields = std::vector<std::pair<std::string, Value>>;

struct Document
{
    uint32_t collectionId = 0;
    uint32_t documentId = 0;
    DocumentFields fields;
};

class Database
{
public:
    explicit Database(const std::string& filename);

    bool initialize();
    std::string execute(const std::string& command);

private:
    bool load();
    bool loadHeader();
    bool loadCollections();
    bool loadFreeList();

    bool saveHeader();
    bool saveCollectionCatalog();
    bool saveFreeList();

    bool createCollection(const std::string& name);
    bool dropCollection(const std::string& name);
    bool collectionExists(const std::string& name) const;
    std::optional<uint32_t> collectionId(const std::string& name) const;

    std::string insertDocument(const std::string& collection,
                               const std::string& objectText);
    std::string selectDocuments(const std::string& collection,
                                const std::vector<std::string>& fields,
                                const std::string& whereClause,
                                const std::string& orderBy,
                                bool descending);
    std::string updateDocuments(const std::string& collection,
                                const std::string& setClause,
                                const std::string& whereClause);
    std::string deleteDocuments(const std::string& collection,
                                const std::string& whereClause);

    bool appendDocument(uint32_t collectionId,
                        const DocumentFields& fields,
                        uint32_t documentId,
                        uint32_t& outDocumentId);
    bool markDocumentDeleted(uint64_t fileOffset);
    bool readDocument(uint64_t fileOffset, Document& document) const;
    std::vector<uint64_t> findDocumentOffsets(uint32_t collectionId,
                                              const std::function<bool(const Document&)>& predicate) const;

    // Scan the data region and rebuild in-memory per-collection offsets
    bool rebuildOffsets();

    DocumentFields parseObject(const std::string& objectText, bool& ok) const;
    Value parseValue(const std::string& token, bool& ok) const;

    bool evaluateWhere(const Document& document, const std::string& whereClause) const;

    void trim(std::string& text) const;
    std::vector<std::string> split(const std::string& text, char delimiter) const;
    std::string toLower(std::string text) const;
    bool startsWith(const std::string& text, const std::string& prefix) const;
    std::string formatValue(const Value& value) const;
    std::string formatDocument(const Document& document) const;
    std::optional<Value> documentFieldValue(const Document& document, const std::string& fieldName) const;
    bool compareValues(const Value& left, const Value& right, const std::string& op) const;

private:
    std::string filename;
    bool initialized = false;

    struct CollectionInfo
    {
        uint32_t id = 0;
        std::string name;
        uint32_t documentCount = 0;
        bool active = true;
        std::vector<uint64_t> docOffsets; // in-memory offsets for fast lookup
    };

    std::vector<CollectionInfo> collections;
    std::vector<uint32_t> freePages;

    struct DatabaseHeaderState
    {
        DatabaseHeader header;
    } headerState;

    static constexpr std::size_t PageSize = 4096;
    static constexpr uint32_t CatalogPage = 1;
    static constexpr uint32_t FreeListPage = 2;
    static constexpr uint32_t FirstDocumentPage = 3;
};

#endif
