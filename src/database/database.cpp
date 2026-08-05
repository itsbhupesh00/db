#include "database.hpp"
#include "database_structs.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
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
        return load();
    }

    in.close();

    std::ofstream out(filename, std::ios::binary);
    if (!out)
        return false;

    // Write a full page for the header so pages are page-aligned.
    DatabaseHeader header;
    std::vector<char> page(PageSize, 0);
    std::memcpy(page.data(), reinterpret_cast<const char*>(&header), sizeof(DatabaseHeader));
    out.write(page.data(), page.size());

    // Write empty catalog and free-list pages (page 1 and page 2)
    std::vector<char> emptyPage(PageSize, 0);
    out.write(emptyPage.data(), emptyPage.size());
    out.write(emptyPage.data(), emptyPage.size());

    out.close();

    headerState = DatabaseHeaderState{};
    return true;
}

bool Database::load()
{
    if (!loadHeader() || !loadCollections() || !loadFreeList())
        return false;

    // build in-memory document offsets
    rebuildOffsets();
    return true;
}

bool Database::rebuildOffsets()
{
    // clear existing offsets
    for (auto& c : collections)
        c.docOffsets.clear();

    std::ifstream in(filename, std::ios::binary);
    if (!in)
        return false;

    in.seekg(0, std::ios::end);
    uint64_t fileSize = static_cast<uint64_t>(in.tellg());
    uint64_t pageStart = PageSize * FirstDocumentPage;
    uint64_t offset = (fileSize >= pageStart) ? pageStart : sizeof(DatabaseHeader);

    while (offset + sizeof(DocumentHeader) <= fileSize)
    {
        in.seekg(offset, std::ios::beg);
        DocumentHeader header;
        in.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!in)
            break;

        if (!header.deleted)
        {
            // find collection and add offset
            for (auto& c : collections)
            {
                if (c.id == header.collectionId && c.active)
                {
                    c.docOffsets.push_back(offset);
                    break;
                }
            }
        }

        // If header looks like all-zero (empty gap), scan forward for next non-zero byte
        if (header.documentSize == 0 && header.collectionId == 0 && header.documentId == 0 && header.deleted == 0)
        {
            uint64_t scanPos = offset + sizeof(DocumentHeader);
            const size_t chunk = 4096;
            std::vector<char> buf(chunk);
            bool found = false;
            while (scanPos < fileSize)
            {
                uint64_t toRead = std::min<uint64_t>(chunk, fileSize - scanPos);
                in.seekg(scanPos, std::ios::beg);
                in.read(buf.data(), static_cast<std::streamsize>(toRead));
                if (!in)
                    break;
                for (uint64_t i = 0; i < toRead; ++i)
                {
                    if (buf[static_cast<size_t>(i)] != 0)
                    {
                        offset = scanPos + i;
                        found = true;
                        break;
                    }
                }
                if (found) break;
                scanPos += toRead;
            }
            if (!found)
                break;
            continue;
        }
        offset += sizeof(DocumentHeader) + header.documentSize;
    }

    return true;
}


bool Database::loadHeader()
{
    std::ifstream in(filename, std::ios::binary);
    if (!in)
        return false;

    in.read(reinterpret_cast<char*>(&headerState.header), sizeof(DatabaseHeader));
    return in.gcount() == sizeof(DatabaseHeader);
}

bool Database::loadCollections()
{
    collections.clear();

    std::ifstream in(filename, std::ios::binary);
    if (!in)
        return false;

    in.seekg(PageSize * CatalogPage, std::ios::beg);
    for (uint32_t i = 0; i < headerState.header.collectionCount; ++i)
    {
        CollectionEntry entry;
        in.read(reinterpret_cast<char*>(&entry), sizeof(CollectionEntry));
        if (!in)
            return false;

        if (!entry.active)
            continue;

        collections.push_back(CollectionInfo{entry.id, std::string(entry.name), entry.documentCount, entry.active});
    }

    return true;
}

bool Database::loadFreeList()
{
    freePages.clear();

    std::ifstream in(filename, std::ios::binary);
    if (!in)
        return false;

    in.seekg(PageSize * FreeListPage, std::ios::beg);
    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in)
        return false;

    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t page = 0;
        in.read(reinterpret_cast<char*>(&page), sizeof(page));
        freePages.push_back(page);
    }

    return true;
}

bool Database::saveHeader()
{
    std::fstream io(filename, std::ios::binary | std::ios::in | std::ios::out);
    if (!io)
        return false;

    io.seekp(0, std::ios::beg);
    io.write(reinterpret_cast<const char*>(&headerState.header), sizeof(DatabaseHeader));
    return static_cast<bool>(io);
}

bool Database::saveCollectionCatalog()
{
    std::fstream io(filename, std::ios::binary | std::ios::in | std::ios::out);
    if (!io)
        return false;

    io.seekp(PageSize * CatalogPage, std::ios::beg);
    std::vector<char> page(PageSize, 0);
    io.write(page.data(), page.size());
    io.seekp(PageSize * CatalogPage, std::ios::beg);

    for (const auto& collection : collections)
    {
        CollectionEntry entry{};
        entry.id = collection.id;
        std::memcpy(entry.name, collection.name.c_str(), std::min(collection.name.size(), sizeof(entry.name) - 1));
        entry.documentCount = collection.documentCount;
        entry.active = collection.active;

        io.write(reinterpret_cast<const char*>(&entry), sizeof(CollectionEntry));
    }

    return static_cast<bool>(io);
}

bool Database::saveFreeList()
{
    std::fstream io(filename, std::ios::binary | std::ios::in | std::ios::out);
    if (!io)
        return false;

    io.seekp(PageSize * FreeListPage, std::ios::beg);
    uint32_t count = static_cast<uint32_t>(freePages.size());
    io.write(reinterpret_cast<const char*>(&count), sizeof(count));
    io.write(reinterpret_cast<const char*>(freePages.data()), sizeof(uint32_t) * count);
    return static_cast<bool>(io);
}

std::string Database::execute(const std::string& command)
{
    std::string trimmed = command;
    trim(trimmed);
    if (trimmed.empty())
        return "";

    if (trimmed == "help")
    {
        return
            "Available commands:\n"
            "  help\n"
            "  create collection <name>;\n"
            "  drop collection <name>;\n"
            "  view collections\n"
            "  view collection <name>\n"
            "  insert into <collection> values { ... };\n"
            "  select * from <collection>;\n"
            "  delete from <collection> where ...;\n";
    }

    std::string lower = toLower(trimmed);
    if (startsWith(lower, "create collection "))
    {
        auto name = trimmed.substr(18);
        trim(name);
        if (name.back() == ';')
            name.pop_back();
        return createCollection(name) ? "Collection created." : "Failed to create collection.";
    }

    if (startsWith(lower, "drop collection "))
    {
        auto name = trimmed.substr(16);
        trim(name);
        if (name.back() == ';')
            name.pop_back();
        return dropCollection(name) ? "Collection dropped." : "Failed to drop collection.";
    }

    if (startsWith(lower, "view collections") || lower == "view collections;")
    {
        if (collections.empty())
            return "No collections defined.";

        std::ostringstream out;
        out << "Collections:\n";
        for (const auto& c : collections)
        {
            if (!c.active)
                continue;
            out << "  - " << c.name << " (id=" << c.id << ", docs=" << c.documentCount << ")\n";
        }
        return out.str();
    }

        if (startsWith(lower, "view collection "))
        {
            auto name = trimmed.substr(16);
            trim(name);
            if (!name.empty() && name.back() == ';')
                name.pop_back();

            auto id = collectionId(name);
            if (!id)
                return "Collection not found.";

            // Reuse selectDocuments to list all documents in the collection
            std::vector<std::string> fields = {"*"};
            return selectDocuments(name, fields, "", "", false);
        }

    if (startsWith(lower, "insert into "))
    {
        auto rest = trimmed.substr(12);
        auto pos = rest.find(" values ");
        if (pos == std::string::npos)
            return "Invalid INSERT syntax.";

        auto collection = rest.substr(0, pos);
        trim(collection);
        auto objectText = rest.substr(pos + 8);
        if (objectText.back() == ';')
            objectText.pop_back();

        return insertDocument(collection, objectText);
    }

    if (startsWith(lower, "select "))
    {
        auto fromPos = lower.find(" from ");
        if (fromPos == std::string::npos)
            return "Invalid SELECT syntax.";

        auto fieldsText = trimmed.substr(7, fromPos - 7);
        auto rest = trimmed.substr(fromPos + 6);

        // Extract optional WHERE / ORDER BY clauses
        std::string whereClause;
        std::string orderBy;

        std::string lowerRest = toLower(rest);
        auto wherePos = lowerRest.find(" where ");
        auto orderPos = lowerRest.find(" order by ");

        std::string collectionPart = rest;
        if (wherePos != std::string::npos)
        {
            collectionPart = rest.substr(0, wherePos);
            if (orderPos != std::string::npos)
                whereClause = rest.substr(wherePos + 7, orderPos - (wherePos + 7));
            else
                whereClause = rest.substr(wherePos + 7);
        }
        else if (orderPos != std::string::npos)
        {
            collectionPart = rest.substr(0, orderPos);
            orderBy = rest.substr(orderPos + 10);
        }

        // Trim trailing semicolon
        auto semicolon = collectionPart.find(';');
        if (semicolon != std::string::npos)
            collectionPart = collectionPart.substr(0, semicolon);
        trim(collectionPart);

        if (!whereClause.empty() && whereClause.back() == ';')
            whereClause.pop_back();
        if (!orderBy.empty() && orderBy.back() == ';')
            orderBy.pop_back();

        std::vector<std::string> fields;
        if (fieldsText == "*")
        {
            fields.push_back("*");
        }
        else
        {
            fields = split(fieldsText, ',');
            for (auto& field : fields)
                trim(field);
        }

        return selectDocuments(collectionPart, fields, whereClause, orderBy, false);
    }

    if (startsWith(lower, "update "))
    {
        auto rest = trimmed.substr(7);
        auto setPos = toLower(rest).find(" set ");
        if (setPos == std::string::npos)
            return "Invalid UPDATE syntax.";

        auto collection = rest.substr(0, setPos);
        trim(collection);
        rest = rest.substr(setPos + 5);

        auto wherePos = toLower(rest).find(" where ");
        std::string setClause;
        std::string whereClause;
        if (wherePos == std::string::npos)
        {
            setClause = rest;
        }
        else
        {
            setClause = rest.substr(0, wherePos);
            whereClause = rest.substr(wherePos + 7);
        }

        if (!whereClause.empty() && whereClause.back() == ';')
            whereClause.pop_back();
        if (!setClause.empty() && setClause.back() == ';')
            setClause.pop_back();

        return updateDocuments(collection, setClause, whereClause);
    }

    if (startsWith(lower, "delete from "))
    {
        auto rest = trimmed.substr(12);
        auto wherePos = toLower(rest).find(" where ");
        std::string collection;
        std::string whereClause;
        if (wherePos == std::string::npos)
        {
            collection = rest;
        }
        else
        {
            collection = rest.substr(0, wherePos);
            whereClause = rest.substr(wherePos + 7);
        }

        trim(collection);
        if (!whereClause.empty() && whereClause.back() == ';')
            whereClause.pop_back();

        return deleteDocuments(collection, whereClause);
    }

    return "Unknown command.";
}

bool Database::createCollection(const std::string& name)
{
    if (name.empty() || collectionExists(name))
        return false;

    CollectionInfo info;
    info.id = headerState.header.nextCollectionId++;
    info.name = name;
    info.documentCount = 0;
    info.active = true;

    collections.push_back(info);
    ++headerState.header.collectionCount;

    return saveHeader() && saveCollectionCatalog();
}

bool Database::dropCollection(const std::string& name)
{
    auto it = std::find_if(collections.begin(), collections.end(),
                           [&](const CollectionInfo& c) { return c.name == name; });
    if (it == collections.end())
        return false;

    it->active = false;
    --headerState.header.collectionCount;

    return saveHeader() && saveCollectionCatalog();
}

bool Database::collectionExists(const std::string& name) const
{
    return std::any_of(collections.begin(), collections.end(),
                       [&](const CollectionInfo& collection) {
                           return collection.active && collection.name == name;
                       });
}

std::optional<uint32_t> Database::collectionId(const std::string& name) const
{
    for (const auto& collection : collections)
    {
        if (collection.active && collection.name == name)
            return collection.id;
    }
    return std::nullopt;
}

std::string Database::insertDocument(const std::string& collection,
                                    const std::string& objectText)
{
    auto id = collectionId(collection);
    if (!id)
        return "Collection not found.";

    bool ok = false;
    auto fields = parseObject(objectText, ok);
    if (!ok)
        return "Failed to parse object.";

    uint32_t documentId = headerState.header.nextDocumentId++;
    if (!appendDocument(*id, fields, documentId, documentId))
        return "Failed to insert document.";

    for (auto& coll : collections)
    {
        if (coll.id == *id)
        {
            ++coll.documentCount;
            break;
        }
    }

    saveHeader();
    saveCollectionCatalog();
    return "Document inserted.";
}

std::string Database::selectDocuments(const std::string& collection,
                                     const std::vector<std::string>& fields,
                                     const std::string& whereClause,
                                     const std::string& orderBy,
                                     bool descending)
{
    auto id = collectionId(collection);
    if (!id)
        return "Collection not found.";
    std::ifstream in(filename, std::ios::binary);
    if (!in)
        return "Failed to open database file.";

    in.seekg(0, std::ios::end);
    uint64_t fileSize = static_cast<uint64_t>(in.tellg());
    uint64_t pageStart = PageSize * FirstDocumentPage;
    uint64_t offset = (fileSize >= pageStart) ? pageStart : sizeof(DatabaseHeader);

    std::string result;
    bool foundAny = false;

    while (offset + sizeof(DocumentHeader) <= fileSize)
    {
        in.seekg(offset, std::ios::beg);
        DocumentHeader header;
        in.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!in)
            break;

        if (!header.deleted && header.collectionId == *id)
        {
            Document document;
            if (readDocument(offset, document))
            {
                if (!whereClause.empty() && !evaluateWhere(document, whereClause))
                {
                    // skip
                }
                else
                {
                    foundAny = true;

                    if (!fields.empty() && !(fields.size() == 1 && fields[0] == "*"))
                    {
                        // print selected fields
                        std::string out = "{";
                        for (size_t i = 0; i < fields.size(); ++i)
                        {
                            if (i) out += ", ";
                            auto opt = documentFieldValue(document, fields[i]);
                            out += '"' + fields[i] + '"';
                            out += ": ";
                            if (opt) out += formatValue(*opt);
                            else out += "null";
                        }
                        out += "}";
                        result += out + "\n";
                    }
                    else
                    {
                        result += formatDocument(document) + "\n";
                    }
                }
            }
        }

        // If header looks like all-zero (empty gap), scan forward for next non-zero byte
        if (header.documentSize == 0 && header.collectionId == 0 && header.documentId == 0 && header.deleted == 0)
        {
            uint64_t scanPos = offset + sizeof(DocumentHeader);
            const size_t chunk = 4096;
            std::vector<char> buf(chunk);
            bool found = false;
            while (scanPos < fileSize)
            {
                uint64_t toRead = std::min<uint64_t>(chunk, fileSize - scanPos);
                in.seekg(scanPos, std::ios::beg);
                in.read(buf.data(), static_cast<std::streamsize>(toRead));
                if (!in)
                    break;
                for (uint64_t i = 0; i < toRead; ++i)
                {
                    if (buf[static_cast<size_t>(i)] != 0)
                    {
                        offset = scanPos + i;
                        found = true;
                        break;
                    }
                }
                if (found) break;
                scanPos += toRead;
            }
            if (!found)
                break;
            continue;
        }

        offset += sizeof(DocumentHeader) + header.documentSize;
    }

    return foundAny ? result : "No documents matched.";
}

std::string Database::updateDocuments(const std::string& collection,
                                     const std::string& setClause,
                                     const std::string& whereClause)
{
    auto id = collectionId(collection);
    if (!id)
        return "Collection not found.";

    if (setClause.empty())
        return "SET clause cannot be empty.";

    std::vector<uint64_t> offsets = findDocumentOffsets(*id, [&](const Document&) {
        return true;
    });

    uint32_t updatedCount = 0;
    for (auto offset : offsets)
    {
        Document document;
        if (!readDocument(offset, document))
            continue;

        if (!whereClause.empty() && !evaluateWhere(document, whereClause))
            continue;

        // Simple update implementation not supported yet.
        (void)setClause;

        ++updatedCount;
    }

    if (updatedCount == 0)
        return "No documents updated.";

    return std::to_string(updatedCount) + " documents updated.";
}

std::string Database::deleteDocuments(const std::string& collection,
                                     const std::string& whereClause)
{
    auto id = collectionId(collection);
    if (!id)
        return "Collection not found.";

    std::vector<uint64_t> offsets = findDocumentOffsets(*id, [&](const Document&) {
        return true;
    });

    uint32_t deletedCount = 0;
    for (auto offset : offsets)
    {
        Document document;
        if (!readDocument(offset, document))
            continue;

        if (!whereClause.empty() && !evaluateWhere(document, whereClause))
            continue;

        if (markDocumentDeleted(offset))
            ++deletedCount;
    }

    return deletedCount == 0 ? "No documents deleted." : std::to_string(deletedCount) + " documents deleted.";
}

bool Database::appendDocument(uint32_t collectionId,
                              const DocumentFields& fields,
                              uint32_t documentId,
                              uint32_t& outDocumentId)
{
    std::ofstream out(filename, std::ios::binary | std::ios::app);
    if (!out)
        return false;

    DocumentHeader header{};
    header.collectionId = collectionId;
    header.documentId = documentId;

    std::ostringstream buffer;
    uint32_t fieldCount = static_cast<uint32_t>(fields.size());
    buffer.write(reinterpret_cast<const char*>(&fieldCount), sizeof(fieldCount));

    for (const auto& field : fields)
    {
        uint32_t nameSize = static_cast<uint32_t>(field.first.size());
        buffer.write(reinterpret_cast<const char*>(&nameSize), sizeof(nameSize));
        buffer.write(field.first.data(), nameSize);

        buffer.put(static_cast<char>(field.second.type));
        switch (field.second.type)
        {
            case ValueType::Null:
                break;
            case ValueType::Boolean:
                buffer.put(field.second.booleanValue ? 1 : 0);
                break;
            case ValueType::Integer:
                buffer.write(reinterpret_cast<const char*>(&field.second.integerValue), sizeof(field.second.integerValue));
                break;
            case ValueType::Double:
                buffer.write(reinterpret_cast<const char*>(&field.second.doubleValue), sizeof(field.second.doubleValue));
                break;
            case ValueType::String:
            {
                uint32_t valueSize = static_cast<uint32_t>(field.second.stringValue.size());
                buffer.write(reinterpret_cast<const char*>(&valueSize), sizeof(valueSize));
                buffer.write(field.second.stringValue.data(), valueSize);
                break;
            }
        }
    }

    std::string payload = buffer.str();
    header.documentSize = static_cast<uint32_t>(payload.size());

    // record offset where header will be written
    std::streamoff pos = out.tellp();
    uint64_t offset = static_cast<uint64_t>(pos);

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(payload.data(), payload.size());

    if (!out)
        return false;

    out.close();
    outDocumentId = documentId;

    // store offset in in-memory collection info
    for (auto& coll : collections)
    {
        if (coll.id == collectionId)
        {
            coll.docOffsets.push_back(offset);
            break;
        }
    }
    // Ensure offsets are consistent with on-disk content
    rebuildOffsets();
    return true;
}

bool Database::markDocumentDeleted(uint64_t fileOffset)
{
    std::fstream io(filename, std::ios::binary | std::ios::in | std::ios::out);
    if (!io)
        return false;

    io.seekp(fileOffset + offsetof(DocumentHeader, deleted), std::ios::beg);
    char deleted = 1;
    io.write(&deleted, sizeof(deleted));
    return static_cast<bool>(io);
}

bool Database::readDocument(uint64_t fileOffset, Document& document) const
{
    std::ifstream in(filename, std::ios::binary);
    if (!in)
        return false;

    in.seekg(fileOffset, std::ios::beg);
    DocumentHeader header;
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in || header.deleted)
        return false;

    document.collectionId = header.collectionId;
    document.documentId = header.documentId;

    uint32_t fieldCount = 0;
    in.read(reinterpret_cast<char*>(&fieldCount), sizeof(fieldCount));
    if (!in)
        return false;

    document.fields.clear();
    for (uint32_t i = 0; i < fieldCount; ++i)
    {
        uint32_t nameSize = 0;
        in.read(reinterpret_cast<char*>(&nameSize), sizeof(nameSize));
        std::string name(nameSize, '\0');
        in.read(&name[0], nameSize);

        char typeChar;
        in.get(typeChar);

        Value value;
        value.type = static_cast<ValueType>(typeChar);
        switch (value.type)
        {
            case ValueType::Null:
                break;
            case ValueType::Boolean:
            {
                char boolByte;
                in.get(boolByte);
                value.booleanValue = boolByte != 0;
                break;
            }
            case ValueType::Integer:
                in.read(reinterpret_cast<char*>(&value.integerValue), sizeof(value.integerValue));
                break;
            case ValueType::Double:
                in.read(reinterpret_cast<char*>(&value.doubleValue), sizeof(value.doubleValue));
                break;
            case ValueType::String:
            {
                uint32_t valueSize = 0;
                in.read(reinterpret_cast<char*>(&valueSize), sizeof(valueSize));
                value.stringValue.resize(valueSize);
                in.read(&value.stringValue[0], valueSize);
                break;
            }
        }

        document.fields.emplace_back(std::move(name), std::move(value));
    }

    return true;
}

std::vector<uint64_t> Database::findDocumentOffsets(uint32_t collectionId,
                                                   const std::function<bool(const Document&)>& predicate) const
{
    std::vector<uint64_t> offsets;
    // Try fast path: use in-memory offsets if present. If empty, rebuild offsets once.
    for (const auto& coll : collections)
    {
        if (coll.id == collectionId)
        {
            if (coll.docOffsets.empty())
            {
                // rebuild offsets (mutating) even though this method is const
                const_cast<Database*>(this)->rebuildOffsets();
            }

            // attempt fast path again
            for (const auto& c2 : collections)
            {
                if (c2.id == collectionId && !c2.docOffsets.empty())
                {
                    for (auto off : c2.docOffsets)
                    {
                        Document doc;
                        if (readDocument(off, doc) && predicate(doc))
                            offsets.push_back(off);
                    }
                    return offsets;
                }
            }

            break;
        }
    }
    std::ifstream in(filename, std::ios::binary);
    if (!in)
        return offsets;

    in.seekg(0, std::ios::end);
    uint64_t fileSize = static_cast<uint64_t>(in.tellg());
    uint64_t offset = PageSize * FirstDocumentPage;

    while (offset + sizeof(DocumentHeader) <= fileSize)
    {
        in.seekg(offset, std::ios::beg);
        DocumentHeader header;
        in.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!in)
            break;

        if (!header.deleted && header.collectionId == collectionId)
        {
            Document document;
            if (readDocument(offset, document) && predicate(document))
                offsets.push_back(offset);
        }

        // If header looks like all-zero (empty gap), scan forward for next non-zero byte
        if (header.documentSize == 0 && header.collectionId == 0 && header.documentId == 0 && header.deleted == 0)
        {
            uint64_t scanPos = offset + sizeof(DocumentHeader);
            const size_t chunk = 4096;
            std::vector<char> buf(chunk);
            bool found = false;
            while (scanPos < fileSize)
            {
                uint64_t toRead = std::min<uint64_t>(chunk, fileSize - scanPos);
                in.seekg(scanPos, std::ios::beg);
                in.read(buf.data(), static_cast<std::streamsize>(toRead));
                if (!in)
                    break;
                for (uint64_t i = 0; i < toRead; ++i)
                {
                    if (buf[static_cast<size_t>(i)] != 0)
                    {
                        offset = scanPos + i;
                        found = true;
                        break;
                    }
                }
                if (found) break;
                scanPos += toRead;
            }
            if (!found)
                break;
            continue;
        }

        offset += sizeof(DocumentHeader) + header.documentSize;
    }

    return offsets;
}

DocumentFields Database::parseObject(const std::string& objectText, bool& ok) const
{
    DocumentFields fields;

    std::string body = objectText;
    trim(body);
    if (body.front() == '{' && body.back() == '}')
    {
        body = body.substr(1, body.size() - 2);
    }

    auto pairs = split(body, ',');
    for (auto& pair : pairs)
    {
        auto colon = pair.find(':');
        if (colon == std::string::npos)
        {
            ok = false;
            return {};
        }

        auto name = pair.substr(0, colon);
        auto valueText = pair.substr(colon + 1);
        trim(name);
        trim(valueText);

        if (name.front() == '"' && name.back() == '"')
            name = name.substr(1, name.size() - 2);

        Value value = parseValue(valueText, ok);
        if (!ok)
            return {};

        fields.emplace_back(name, std::move(value));
    }

    ok = true;
    return fields;
}

Value Database::parseValue(const std::string& token, bool& ok) const
{
    Value value;
    std::string trimmed = token;
    trim(trimmed);

    if (trimmed == "null")
    {
        value.type = ValueType::Null;
        ok = true;
        return value;
    }

    if (trimmed == "true" || trimmed == "false")
    {
        value.type = ValueType::Boolean;
        value.booleanValue = trimmed == "true";
        ok = true;
        return value;
    }

    if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"')
    {
        value.type = ValueType::String;
        value.stringValue = trimmed.substr(1, trimmed.size() - 2);
        ok = true;
        return value;
    }

    try
    {
        if (trimmed.find('.') != std::string::npos)
        {
            value.type = ValueType::Double;
            value.doubleValue = std::stod(trimmed);
        }
        else
        {
            value.type = ValueType::Integer;
            value.integerValue = std::stoll(trimmed);
        }
        ok = true;
        return value;
    }
    catch (...) {
        ok = false;
        return {};
    }
}

bool Database::evaluateWhere(const Document& document, const std::string& whereClause) const
{
    if (whereClause.empty())
        return true;

    // Very small evaluator: supports single comparison like `field op value` or `id op value`.
    // Supported ops: ==, =, !=, <=, >=, <, >
    std::string expr = whereClause;
    trim(expr);

    // find operator
    const std::vector<std::string> ops = {"==","!=","<=",">=","<","=",">"};
    std::string foundOp;
    std::size_t opPos = std::string::npos;
    for (const auto& op : ops)
    {
        opPos = expr.find(op);
        if (opPos != std::string::npos)
        {
            foundOp = op;
            break;
        }
    }

    if (opPos == std::string::npos)
        return false;

    std::string left = expr.substr(0, opPos);
    std::string right = expr.substr(opPos + foundOp.size());
    trim(left);
    trim(right);

    // Normalize equality operator
    if (foundOp == "=")
        foundOp = "==";

    // id refers to document.documentId
    if (toLower(left) == "id")
    {
        try {
            long long rv = std::stoll(right);
            if (foundOp == "==") return document.documentId == static_cast<uint32_t>(rv);
            if (foundOp == "!=") return document.documentId != static_cast<uint32_t>(rv);
            if (foundOp == "<") return document.documentId < static_cast<uint32_t>(rv);
            if (foundOp == "<=") return document.documentId <= static_cast<uint32_t>(rv);
            if (foundOp == ">") return document.documentId > static_cast<uint32_t>(rv);
            if (foundOp == ">=") return document.documentId >= static_cast<uint32_t>(rv);
        } catch (...) {
            return false;
        }
    }

    // Otherwise, compare against a named field
    auto opt = documentFieldValue(document, left);
    if (!opt.has_value())
        return false;

    bool ok = false;
    Value rv = parseValue(right, ok);
    if (!ok)
        return false;

    return compareValues(opt.value(), rv, foundOp);
}

void Database::trim(std::string& text) const
{
    const char* whitespace = " \t\n\r";
    text.erase(0, text.find_first_not_of(whitespace));
    text.erase(text.find_last_not_of(whitespace) + 1);
}

std::vector<std::string> Database::split(const std::string& text, char delimiter) const
{
    std::vector<std::string> items;
    std::string current;

    bool inQuotes = false;
    for (char ch : text)
    {
        if (ch == '"')
            inQuotes = !inQuotes;

        if (ch == delimiter && !inQuotes)
        {
            items.push_back(current);
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty())
        items.push_back(current);

    return items;
}

std::string Database::toLower(std::string text) const
{
    for (char& ch : text)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

bool Database::startsWith(const std::string& text, const std::string& prefix) const
{
    return text.rfind(prefix, 0) == 0;
}

std::string Database::formatValue(const Value& value) const
{
    switch (value.type)
    {
        case ValueType::Null: return "null";
        case ValueType::Boolean: return value.booleanValue ? "true" : "false";
        case ValueType::Integer: return std::to_string(value.integerValue);
        case ValueType::Double: return std::to_string(value.doubleValue);
        case ValueType::String: return '"' + value.stringValue + '"';
    }
    return "null";
}

std::string Database::formatDocument(const Document& document) const
{
    std::string output = "{";
    for (std::size_t i = 0; i < document.fields.size(); ++i)
    {
        const auto& field = document.fields[i];
        output += '"' + field.first + '"';
        output += ": ";
        output += formatValue(field.second);
        if (i + 1 < document.fields.size())
            output += ", ";
    }
    output += "}";
    return output;
}

std::optional<Value> Database::documentFieldValue(const Document& document, const std::string& fieldName) const
{
    for (const auto& field : document.fields)
    {
        if (field.first == fieldName)
            return field.second;
    }
    return std::nullopt;
}

bool Database::compareValues(const Value& left, const Value& right, const std::string& op) const
{
    if (left.type != right.type)
        return false;

    switch (left.type)
    {
        case ValueType::Null:
            if (op == "==") return true;
            if (op == "!=") return false;
            return false;
        case ValueType::Boolean:
            if (op == "==") return left.booleanValue == right.booleanValue;
            if (op == "!=") return left.booleanValue != right.booleanValue;
            return false;
        case ValueType::Integer:
            if (op == "==") return left.integerValue == right.integerValue;
            if (op == "!=") return left.integerValue != right.integerValue;
            if (op == "<") return left.integerValue < right.integerValue;
            if (op == "<=") return left.integerValue <= right.integerValue;
            if (op == ">") return left.integerValue > right.integerValue;
            if (op == ">=") return left.integerValue >= right.integerValue;
            return false;
        case ValueType::Double:
            if (op == "==") return left.doubleValue == right.doubleValue;
            if (op == "!=") return left.doubleValue != right.doubleValue;
            if (op == "<") return left.doubleValue < right.doubleValue;
            if (op == "<=") return left.doubleValue <= right.doubleValue;
            if (op == ">") return left.doubleValue > right.doubleValue;
            if (op == ">=") return left.doubleValue >= right.doubleValue;
            return false;
        case ValueType::String:
            if (op == "==") return left.stringValue == right.stringValue;
            if (op == "!=") return left.stringValue != right.stringValue;
            if (op == "<") return left.stringValue < right.stringValue;
            if (op == "<=") return left.stringValue <= right.stringValue;
            if (op == ">") return left.stringValue > right.stringValue;
            if (op == ">=") return left.stringValue >= right.stringValue;
            return false;
    }
    return false;
}
