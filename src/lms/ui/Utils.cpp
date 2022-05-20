/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "Utils.hpp"

#include <iomanip>
#include <sstream>

#include <Wt/WText.h>

#include "services/database/Cluster.hpp"
#include "services/database/Session.hpp"
#include "LmsApplication.hpp"

namespace UserInterface::Utils
{

	std::string
	durationToString(std::chrono::milliseconds msDuration)
	{
		const std::chrono::seconds duration {std::chrono::duration_cast<std::chrono::seconds>(msDuration)};

		std::ostringstream oss;
		oss << (duration.count() / 60)
			<< ":"
			<< std::setfill('0') << std::setw(2) << duration.count() % 60;

		return oss.str();
	}


	std::unique_ptr<Wt::WImage>
	createCover(Database::ReleaseId releaseId, CoverResource::Size size)
	{
		auto cover {std::make_unique<Wt::WImage>()};
		cover->setImageLink(LmsApp->getCoverResource()->getReleaseUrl(releaseId, size));
		cover->setStyleClass("Lms-cover img-fluid"); // HACK
		cover->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)"); // HACK
		return cover;
	}

	std::unique_ptr<Wt::WImage>
	createCover(Database::TrackId trackId, CoverResource::Size size)
	{
		auto cover {std::make_unique<Wt::WImage>()};
		cover->setImageLink(LmsApp->getCoverResource()->getTrackUrl(trackId, size));
		cover->setStyleClass("Lms-cover img-fluid"); // HACK
		cover->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)"); // HACK
		return cover;
	}

	std::unique_ptr<Wt::WInteractWidget>
	createCluster(Database::ClusterId clusterId, bool canDelete)
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const Database::Cluster::pointer cluster {Database::Cluster::find(LmsApp->getDbSession(), clusterId)};
		if (!cluster)
			return {};

		auto getStyleClass {[](const Database::Cluster::pointer& cluster) -> const char*
		{
			switch (cluster->getType()->getId().getValue() % 8)
			{
				case 0: return "bg-primary";
				case 1: return "bg-secondary";
				case 2: return "bg-success";
				case 3: return "bg-danger";
				case 4: return "bg-warning text-dark";
				case 5: return "bg-info text-dark";
				case 6: return "bg-light text-dark";
				case 7: return "bg-dark";
			}

			return "bg-primary";
		}};

		const std::string styleClass {getStyleClass(cluster)};
		auto res {std::make_unique<Wt::WText>(std::string {} + (canDelete ? "<i class=\"fa fa-times-circle\"></i> " : "") + Wt::WString::fromUTF8(cluster->getName()), Wt::TextFormat::UnsafeXHTML)};

		res->setStyleClass("Lms-badge-cluster badge me-1 " + styleClass); // HACK
		res->setToolTip(cluster->getType()->getName(), Wt::TextFormat::Plain);
		res->setInline(true);

		return res;
	}
}
