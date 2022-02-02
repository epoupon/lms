#pragma once

#include <functional>
#include "services/database/ArtistId.hpp"
#include "services/database/ReleaseId.hpp"
#include "services/database/TrackId.hpp"

namespace Recommendation
{
	struct Progress
	{
		std::size_t totalElems {};
		std::size_t	processedElems {};
	};
	using ProgressCallback = std::function<void(const Progress&)>;

	template <typename IdType>
	using ResultContainer = std::vector<IdType>;

	using ArtistContainer = ResultContainer<Database::ArtistId>;
	using ReleaseContainer = ResultContainer<Database::ReleaseId>;
	using TrackContainer = ResultContainer<Database::TrackId>;

} // namespace Recommendation
