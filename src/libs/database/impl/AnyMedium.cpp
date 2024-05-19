#include "database/AnyMedium.hpp"

#include <database/Session.hpp>
#include <tuple>

#include "Utils.hpp"

using namespace lms::db;

namespace
{
    AnyMediumId fromString(const std::string &type, const Wt::Dbo::dbo_default_traits::IdType id)
    {
        if (type == "artist")
            return ArtistId(id);
        if (type == "release")
            return ReleaseId(id);
        if (type == "track")
            return TrackId(id);

        throw std::logic_error("unknown medium type");
    }
}

RangeResults<AnyMediumId> any_medium::findIds(Session &session, const std::vector<std::string_view> &keywords, std::optional<Range> range)
{
    using Columns = std::tuple<std::string, Wt::Dbo::dbo_default_traits::IdType, int>;

    session.checkReadTransaction();

    auto query = session.getDboSession()->query<Columns>(R"(
        SELECT type, id, sum(weight) AS v
        FROM keywords
    )");

    for (const std::string_view keyword : keywords)
    {
        query.orWhere("value LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind("%" + utils::escapeLikeKeyword(keyword) + "%");
    }

    query
        .groupBy("type, id")
        .orderBy("v DESC");

    auto columns = utils::execRangeQuery<Columns>(query, range);
    auto results = std::vector<AnyMediumId>(columns.results.size());

    for (const auto&[type, id, _] : columns.results)
        results.emplace_back(fromString(type, id));

    return {
        columns.range,
        results,
        columns.moreResults
    };
}
