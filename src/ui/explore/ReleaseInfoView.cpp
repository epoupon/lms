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

#include "ReleaseInfoView.hpp"

#include "database/Release.hpp"
#include "main/Services.hpp"
#include "similarity/SimilaritySearcher.hpp"
#include "utils/Utils.hpp"

#include "ReleaseLink.hpp"
#include "LmsApplication.hpp"

using namespace Database;

namespace UserInterface {

ReleaseInfo::ReleaseInfo()
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.ReleaseInfo.template"))
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_similarReleasesContainer = bindNew<Wt::WContainerWidget>("similar-releases");

	wApp->internalPathChanged().connect(std::bind([=]
	{
		refresh();
	}));

	LmsApp->getEvents().dbScanned.connect([=]
	{
		refresh();
	});

	refresh();
}

void
ReleaseInfo::refresh()
{
	_similarReleasesContainer->clear();

	if (!wApp->internalPathMatches("/release/"))
		return;

	auto releaseId = readAs<Database::IdType>(wApp->internalPathNextPart("/release/"));
	if (!releaseId)
		return;

	auto releasesIds = getServices().similaritySearcher->getSimilarReleases(LmsApp->getDboSession(), *releaseId, 5);

	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	std::vector<Database::Release::pointer> releases;
	for (auto releaseId : releasesIds)
	{
		auto release = Database::Release::getById(LmsApp->getDboSession(), releaseId);

		if (release)
			releases.push_back(release);
	}

	for (auto release : releases)
		_similarReleasesContainer->addNew<ReleaseLink>(release);
}

} // namespace UserInterface

