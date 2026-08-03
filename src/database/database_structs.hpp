#ifndef DATABASE_STRUCTS_HPP
#define DATABASE_STRUCTS_HPP

#include <cstdint>

struct DatabaseHeader
{
    char magic[8] = {'B','I','N','D','B','0','0','1'};
    uint32_t version = 1;
    uint32_t collectionCount = 0;
    char reserved[48] = {};
};

struct CollectionEntry
{
    uint32_t id = 0;
    char name[32] = {};
    uint32_t documentCount = 0;
    bool active = true;
    char reserved[23] = {};
};

struct DocumentHeader
{
    uint32_t collectionId = 0;
    uint32_t documentSize = 0;
    bool deleted = false;
};

#endif
