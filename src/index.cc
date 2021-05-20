#include "index.h"

#include "decoder.h"

std::string_view SymbolTable::lookup(size_t index) const {
    if (index == 0) {
        throw std::domain_error("SymbolTable: 0 is an impossible index");
    }
    if (index > symbols.size()) {
        throw std::domain_error("SymbolTable: too high index");
    }
    // for some reason symbols are treated as 1-indexed. adjust accordingly.
    return symbols.at(index - 1);
}

void SymbolTable::load(Decoder& dec) {
    dec.read_int<uint32_t>(); // len
    auto numSymbols = dec.read_int<uint32_t>();

    symbols.reserve(numSymbols);

    std::string value;
    for (int i = 0; i < numSymbols; ++i) {
        auto strLen = dec.read_varuint();
        if (strLen == 0) {
            continue;
        }
        value.resize(strLen);
        dec.read(value.data(), strLen);
        symbols.push_back(value);
    }
}

void TOC::load(Decoder& dec) {
    dec.read_int_to(symbol_offset);
    dec.read_int_to(series_offset);
    dec.read_int_to(label_indices_offset);
    dec.read_int_to(label_offset_table_offset);
    dec.read_int_to(postings_start_offset);
    dec.read_int_to(postings_offset_table_offset);
}

ChunkReference& ChunkReference::operator+=(const ChunkReference& other) {
    minTime += other.minTime;
    maxTime += other.maxTime;
    fileReference += other.fileReference;
    return *this;
}

uint32_t ChunkReference::getSegmentFileId() const {
    return uint32_t(fileReference >> 32) + 1;
}
uint32_t ChunkReference::getOffset() const {
    return fileReference & 0xFFFFFFFF;
}

void Series::load(Decoder& dec, const SymbolTable& symbols) {
    auto len = dec.read_varuint(); // maybe?
    auto labelCount = dec.read_varuint();

    for (int i = 0; i < labelCount; ++i) {
        auto name_id = dec.read_varuint();
        auto value_id = dec.read_varuint();
        labels.emplace(symbols.lookup(name_id), symbols.lookup(value_id));
    }

    auto chunkCount = dec.read_varuint();
    if (chunkCount == 0) {
        // this may be valid during compaction, but isn't handled here
        throw std::runtime_error("Series with no chunks isn't handled");
    }

    chunks.reserve(chunkCount);

    {
        ChunkReference first;

        first.minTime = dec.read_varint(); //// IS SIGNED
        first.maxTime = dec.read_varuint() + first.minTime;
        first.fileReference = dec.read_varuint();

        chunks.push_back(std::move(first));
    }

    for (int i = 1; i < chunkCount; ++i) {
        ChunkReference chunk;
        chunk.minTime = dec.read_varint(); /// IS SIGNED
        chunk.maxTime = dec.read_varuint();
        chunk.fileReference = dec.read_varint(); /// IS SIGNED

        chunk += chunks.back();
        chunks.push_back(std::move(chunk));
    }

    dec.read_int<uint32_t>(); // CRC
}

void SeriesTable::load(Decoder& dec,
                       const SymbolTable& symbols,
                       size_t expectedEnd) {
    while (dec.consume_to_alignment(16) < expectedEnd) {
        auto offset = dec.tellg();
        size_t id = offset / 16;
        Series s;
        s.load(dec, symbols);
        series[id] = std::move(s);
    }
}

void Index::load(Decoder& dec) {
    dec.seekg(-(8 * 6 + 4), std::ios::end);

    toc.load(dec);

    if (!toc.symbol_offset) {
        throw std::runtime_error("No symbol table in index file");
    }

    dec.seekg(toc.symbol_offset);
    symbols.load(dec);

    if (!toc.series_offset) {
        throw std::runtime_error("No series in index file");
    }

    dec.seekg(toc.series_offset);
    series.load(dec, symbols, toc.label_indices_offset);
}