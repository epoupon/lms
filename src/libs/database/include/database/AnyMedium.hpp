#pragma once

#include <variant>
#include <vector>
#include <optional>

#include "ArtistId.hpp"
#include "ClusterId.hpp"
#include "MediaLibraryId.hpp"
#include "ReleaseId.hpp"
#include "TrackId.hpp"
#include "Types.hpp"


namespace lms::db
{
    class Session;
    using AnyMediumId = std::variant<ArtistId, ReleaseId, TrackId>;

    std::ostream& operator<<(std::ostream& os, const AnyMediumId& v);

    namespace any_medium
    {

        AnyMediumId fromString(const std::string &type, Wt::Dbo::dbo_default_traits::IdType id);
        RangeResults<AnyMediumId> findIds(Session &session, const std::vector<std::string_view> &keywords,
                                          std::span<const ClusterId> clusters, MediaLibraryId mediaLibrary,
                                          std::optional<Range> range);
    }
}
