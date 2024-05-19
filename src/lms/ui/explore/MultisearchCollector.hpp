#pragma once

#include <optional>
#include <variant>

#include "database/TrackId.hpp"
#include "database/ArtistId.hpp"
#include "database/ReleaseId.hpp"
#include "DatabaseCollectorBase.hpp"


namespace lms::ui
{
	using MediumId = std::variant<db::TrackId, db::ArtistId, db::ReleaseId>;

	class MultisearchCollector : public DatabaseCollectorBase
	{
		public:
			using DatabaseCollectorBase::DatabaseCollectorBase;

			[[nodiscard]] db::RangeResults<MediumId> get(const std::optional<db::Range>& requestedRange = std::nullopt) const ;

		private:
			[[nodiscard]] db::RangeResults<db::TrackId> getTracks(const std::optional<db::Range>& requestedRange) const ;
			[[nodiscard]] db::RangeResults<db::ReleaseId> getReleases(const std::optional<db::Range>& requestedRange) const ;
			[[nodiscard]] db::RangeResults<db::ArtistId> getArtists(const std::optional<db::Range>& requestedRange) const ;
	};
} // ns UserInterface

