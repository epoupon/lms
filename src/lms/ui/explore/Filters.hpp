/*
 * Copyright (C) 2018 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <vector>
#include <Wt/WContainerWidget.h>
#include <Wt/WSignal.h>
#include <Wt/WTemplate.h>

#include "services/database/ClusterId.hpp"

#include "Filters.hpp"

namespace UserInterface
{
	class Filters : public Wt::WTemplate
	{
		public:
			Filters();

			void add(Database::ClusterId clusterId);
			const std::vector<Database::ClusterId>& getClusterIds() const { return _clusterIds; }
			Wt::Signal<>& updated() { return _sigUpdated; }

		private:
			void showDialog();

			Wt::WContainerWidget *_filters;
			Wt::Signal<> _sigUpdated;
			std::vector<Database::ClusterId> _clusterIds;
	};
} // namespace UserInterface

