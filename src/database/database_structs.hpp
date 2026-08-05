#ifndef DATABASE_STRUCTS_HPP
#define DATABASE_STRUCTS_HPP

#include <cstdint>
#include <cstddef>

constexpr std::size_t MaxCollectionNameLength = 32;

struct DatabaseHeader
{
    char magic[8] = {'B','I','N','D','B','0','0','1'};
    uint32_t version = 1;
    uint32_t collectionCount = 0;
    uint32_t nextCollectionId = 1;
    uint32_t nextDocumentId = 1;
    char reserved[40] = {};
};

struct CollectionEntry
{
    uint32_t id = 0;
    char name[MaxCollectionNameLength] = {};
    uint32_t documentCount = 0;
    bool active = true;
    char reserved[23] = {};
};

struct DocumentHeader
{
    uint32_t collectionId = 0;
    uint32_t documentId = 0;
    uint32_t documentSize = 0;
    bool deleted = false;
    char reserved[3] = {};
};

static_assert(sizeof(DatabaseHeader) == 64, "DatabaseHeader must remain 64 bytes");
static_assert(sizeof(CollectionEntry) == 64, "CollectionEntry must remain 64 bytes");
static_assert(sizeof(DocumentHeader) == 16, "DocumentHeader must remain 16 bytes");

#endif
