#include "MultisearchCollector.hpp"

#include <LmsApplication.hpp>
#include <database/Session.hpp>

#include "Filters.hpp"

namespace lms::ui
{
    using namespace db;

    // TODO: apply filters
    RangeResults<AnyMediumId> MultisearchCollector::get(const std::optional<Range>& requestedRange) const
    {
        const Range range = getActualRange(requestedRange);

        auto transaction = LmsApp->getDbSession().createReadTransaction();
        auto results = any_medium::findIds(LmsApp->getDbSession(), getSearchKeywords(), getFilters().getClusters(), getFilters().getMediaLibrary(), range);

        if (range.offset + range.size >= getMaxCount())
            results.moreResults = false;

        return results;
    }
} // ns UserInterface
