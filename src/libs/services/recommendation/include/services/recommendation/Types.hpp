#pragma once

#include <functional>

#include "database/objects/ArtistId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"

namespace lms::recommendation
{
    struct Progress
    {
        std::size_t totalElems{};
        std::size_t processedElems{};
    };
    using ProgressCallback = std::function<void(const Progress&)>;

    template<typename IdType>
    using ResultContainer = std::vector<IdType>;

    using ArtistContainer = ResultContainer<db::ArtistId>;
    using ReleaseContainer = ResultContainer<db::ReleaseId>;
    using TrackContainer = ResultContainer<db::TrackId>;

} // namespace lms::recommendation
