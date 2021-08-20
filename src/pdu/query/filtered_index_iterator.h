#pragma once

#include "../io/chunk_file_cache.h"
#include "../io/index.h"
#include "../io/series_sample_iterator.h"
#include "../query/series_filter.h"
#include "../util/iterator_facade.h"

#include <memory>
#include <set>

struct SeriesHandle {
    const Series* series;
    SeriesSampleIterator sampleItr;
};

class FilteredIndexIterator
    : public iterator_facade<FilteredIndexIterator, SeriesHandle> {
public:
    FilteredIndexIterator(const std::shared_ptr<Index>& index,
                          const SeriesFilter& filter);

    FilteredIndexIterator(const FilteredIndexIterator& other);

    void increment();

    const SeriesHandle& dereference() const {
        return handle;
    }

    bool is_end() const {
        return refItr == filteredSeriesRefs.end();
    }

private:
    // update the SeriesHandle to point to the series referenced by the current
    // value of refItr
    void update();

    std::shared_ptr<const Series> getCurrentSeries() const {
        // This creates an aliasing shared ptr. It can be used to access the
        // const Series it points to, but it shares the ownership information
        // with the index.
        // This prolongs the index's life until there are no users of any
        // of its contained series.
        // This is _primarily_ to reduce the possiblity of screwing up lifetimes
        // in the Python bindings; for C++ this is relatively unnecessary as
        // it is reasonable to expect destroying a container (the index) to
        // invalidate interators and references to its contents.
        return std::shared_ptr<const Series>(index, &index->series.at(*refItr));
    }

    std::shared_ptr<Index> index;
    std::shared_ptr<ChunkFileCache> cache;
    std::set<size_t> filteredSeriesRefs;
    std::set<size_t>::const_iterator refItr;
    SeriesHandle handle;
};