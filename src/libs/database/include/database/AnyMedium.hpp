#pragma once

#include <variant>
#include <vector>
#include <optional>

#include "ArtistId.hpp"
#include "ReleaseId.hpp"
#include "TrackId.hpp"
#include "Types.hpp"

namespace lms::db
{
    class Session;
    using AnyMediumId = std::variant<ArtistId, ReleaseId, TrackId>;

    namespace any_medium
    {
        RangeResults<AnyMediumId> findIds(Session& session, const std::vector<std::string_view>& keywords, std::optional<Range> range);
    }
}
