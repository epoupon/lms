#pragma once

#include <optional>
#include <database/AnyMedium.hpp>

#include "database/TrackId.hpp"
#include "database/ArtistId.hpp"
#include "database/ReleaseId.hpp"
#include "DatabaseCollectorBase.hpp"


namespace lms::ui
{
	class MultisearchCollector : public DatabaseCollectorBase
	{
		public:
			using DatabaseCollectorBase::DatabaseCollectorBase;

			[[nodiscard]] db::RangeResults<db::AnyMediumId> get(const std::optional<db::Range>& requestedRange = std::nullopt) const ;
	};
} // ns UserInterface

