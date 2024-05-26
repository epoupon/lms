#pragma once

#include <optional>
#include <database/AnyMedium.hpp>

#include "DatabaseCollectorBase.hpp"


namespace lms::ui
{
	class MultisearchCollector : public DatabaseCollectorBase
	{
		public:
			using DatabaseCollectorBase::DatabaseCollectorBase;

			[[nodiscard]] db::RangeResults<db::AnyMediumId> get(db::any_medium::Type filter, const std::optional<db::Range>& requestedRange = std::nullopt) const ;
	};
} // ns UserInterface

