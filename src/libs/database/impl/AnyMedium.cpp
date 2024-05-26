#include "database/AnyMedium.hpp"

#include <ostream>
#include <tuple>

#include <database/Session.hpp>

#include "Utils.hpp"

using namespace lms::db;

AnyMediumId any_medium::fromString(const std::string &type, const Wt::Dbo::dbo_default_traits::IdType id)
{
    if (type == "artist")
        return ArtistId(id);
    if (type == "release")
        return ReleaseId(id);
    if (type == "track")
        return TrackId(id);

    throw std::logic_error("unknown medium type");
}

RangeResults<AnyMediumId> any_medium::findIds(Session &session, Type type, const std::vector<std::string_view> &keywords,
                                          std::span<const ClusterId> clusters, MediaLibraryId mediaLibrary,
                                          std::optional<Range> range)
{
    using Columns = std::tuple<std::string, Wt::Dbo::dbo_default_traits::IdType, int>;

    session.checkReadTransaction();

    auto media_library_query = session.getDboSession()->query<Wt::Dbo::dbo_default_traits::IdType>("SELECT json_each.value FROM json_each(media_library_ids)")
    .where("json_each.value = ?").bind(mediaLibrary.getValue());

    auto cluster_query = session.getDboSession()->query<Wt::Dbo::dbo_default_traits::IdType>("SELECT json_each.value FROM json_each(cluster_ids)");
    for (auto cluster_id: clusters)
        cluster_query.orWhere("json_each.value = ?").bind(cluster_id.getValue());

    auto query = session.getDboSession()->query<Columns>(R"(
        SELECT type, id, sum(weight) AS v
        FROM keywords
    )");

    for (const std::string_view keyword : keywords)
    {
        query.orWhere("value LIKE ? ESCAPE '" ESCAPE_CHAR_STR "'").bind("%" + utils::escapeLikeKeyword(keyword) + "%");
    }

    if (mediaLibrary != MediaLibraryId{})
        query.where("EXISTS(" + media_library_query.asString() + ")").bindSubqueryValues(media_library_query);

    if (!clusters.empty())
        query.where("EXISTS(" + cluster_query.asString() + ")").bindSubqueryValues(cluster_query);

    switch (type)
    {
        case Type::ALL:
            break;
        case Type::RELEASES:
            query.where("type = 'release'");
            break;
        case Type::ARTISTS:
            query.where("type = 'artist'");
            break;
        case Type::TRACKS:
            query.where("type = 'track'");
            break;
    }

    query
        .groupBy("type, id")
        .orderBy("v DESC");

    auto columns = utils::execRangeQuery<Columns>(query, range);

    auto results = std::vector<AnyMediumId>();
    results.reserve(columns.results.size());
    for (const auto&[type, id, _] : columns.results)
        results.emplace_back(fromString(type, id));

    return {
        columns.range,
        results,
        columns.moreResults
    };
}

std::ostream & lms::db::operator<<(std::ostream &os, const AnyMediumId &v)
{
    std::visit([&os](auto&& arg)
    {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, ArtistId>)
            os << "Artist(" << arg.getValue() << ")";
        else if constexpr (std::is_same_v<T, ReleaseId>)
            os << "Release(" << arg.getValue() << ")";
        else if constexpr (std::is_same_v<T, TrackId>)
            os << "Track(" << arg.getValue() << ")";
        else
            static_assert(false, "inexhaustible patterns");

    }, v);
    return os;
}
