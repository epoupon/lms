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

#include <Wt/WAnchor.h>

#include "database/Release.hpp"
#include "main/Service.hpp"
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
	setCondition("if-has-copyright-or-copyright-url", false);
	setCondition("if-has-copyright-url", false);
	setCondition("if-has-copyright", false);

	if (!wApp->internalPathMatches("/release/"))
		return;

	auto releaseId {readAs<Database::IdType>(wApp->internalPathNextPart("/release/"))};
	if (!releaseId)
		return;

	const std::vector<Database::IdType> releasesIds {getService<Similarity::Searcher>()->getSimilarReleases(LmsApp->getDbSession(), *releaseId, 5)};

	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	Database::Release::pointer release {Database::Release::getById(LmsApp->getDbSession(), *releaseId)};
	if (!release)
		return;

	std::optional<std::string> copyright {release->getCopyright()};
	std::optional<std::string> copyrightURL {release->getCopyrightURL()};

	setCondition("if-has-copyright-or-copyright-url", copyright || copyrightURL);

	if (copyrightURL)
	{
		setCondition("if-has-copyright-url", true);

		Wt::WLink link {*copyrightURL};
		link.setTarget(Wt::LinkTarget::NewWindow);

		Wt::WAnchor* anchor {bindNew<Wt::WAnchor>("copyright-url", link)};
		anchor->setTextFormat(Wt::TextFormat::XHTML);
		anchor->setText(Wt::WString::tr("Lms.Explore.Release.template.link-btn"));
	}

	if (copyright)
	{
		setCondition("if-has-copyright", true);
		bindString("copyright", Wt::WString::fromUTF8(*copyright), Wt::TextFormat::Plain);
	}

	std::vector<Database::Release::pointer> similarReleases;
	for (Database::IdType id : releasesIds)
	{
		Database::Release::pointer similarRelease {Database::Release::getById(LmsApp->getDbSession(), id)};

		if (similarRelease)
			similarReleases.emplace_back(similarRelease);
	}

	for (const Database::Release::pointer& similarRelease : similarReleases)
		_similarReleasesContainer->addNew<ReleaseLink>(similarRelease);
}

} // namespace UserInterface

