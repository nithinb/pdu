#pragma once

#include "chunk_file_cache.h"
#include "chunk_reference.h"
#include "decoder.h"
#include "index.h"
#include "resource.h"

#include <boost/filesystem.hpp>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

inline constexpr uint32_t HeadChunkFileMagic = 0x0130BC91;

inline constexpr size_t HeadChunkMetaMinLen = 8 + 8 + 8 + 1 + 1 + 4;

inline constexpr size_t PageSize = 32 * 1024;

enum RecordType {
    PageEmpty = 0b0000,
    RecordFull = 0b0001,
    RecordStart = 0b0010,
    RecordMid = 0b0011,
    RecordEnd = 0b0100,
    Compressed = 0b1000
};

class InMemWalChunk {
public:
    InMemWalChunk();

    void setMinTime(int64_t ts);
    void addSample(int64_t ts, double value);

    std::pair<std::shared_ptr<Resource>, ChunkReference> makeResource() const;

    // private:
    std::vector<uint8_t> data;
    uint64_t minTime;
    uint64_t maxTime;
};

class WalLoader {
public:
    WalLoader(std::map<size_t, Series>& series,
              std::set<std::string, std::less<>>& symbols,
              std::map<size_t, InMemWalChunk>& walChunks)
        : seriesMap(series), symbols(symbols), walChunks(walChunks) {
    }
    void load(const boost::filesystem::path& file);

    void clear() {
        rawBuffer.clear();
        decompressedBuffer.clear();
        needsDecompressing = false;
    }

private:
    void loadFile(const boost::filesystem::path& file, bool isLast = false);
    void loadFragment(Decoder& dec, bool isLastFile);
    void loadRecord(Decoder dec);
    void loadSeries(Decoder& dec);
    void loadSamples(Decoder& dec);

    /**
     * copy a provided symbol into the symbol set, and return a view
     * to the stored symbol.
     *
     * Series take string view labels, so the underlying data needs to be stored
     * in the symbol table.
     */
    std::string_view addSymbol(std::string_view sym);

    std::map<size_t, Series>& seriesMap;
    std::set<std::string, std::less<>>& symbols;
    std::map<size_t, InMemWalChunk>& walChunks;

    std::vector<uint8_t> rawBuffer;
    std::vector<uint8_t> decompressedBuffer;
    bool needsDecompressing = false;
};

class HeadChunks {
public:
    HeadChunks(const boost::filesystem::path& dataDir);

    // private:
    void loadChunkFile(Decoder& dec, size_t fileId);

    std::shared_ptr<ChunkFileCache> cache;

    // seriesRef to chunk references
    std::map<size_t, Series> seriesMap;
    // storage for strings referenced from the wal.
    std::set<std::string, std::less<>> symbols;
    // storage for data read from the wal
    std::map<size_t, InMemWalChunk> walChunks;
};