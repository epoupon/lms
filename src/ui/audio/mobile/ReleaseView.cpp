/*
 * Copyright (C) 2015 Emeric Poupon
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

#include <Wt/WText>
#include <Wt/WImage>
#include <Wt/WPushButton>
#include <Wt/WTemplate>

#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include "LmsApplication.hpp"

#include "ReleaseView.hpp"

namespace UserInterface {
namespace Mobile {

using namespace Database;

ReleaseView::ReleaseView(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent)
{
	Wt::WTemplate* wrapper = new Wt::WTemplate(this);
	wrapper->setTemplateText(Wt::WString::tr("wa-trackview-wrapper"));

	Wt::WTemplate* title = new Wt::WTemplate(this);
	wrapper->bindWidget("title", title);

	title->setTemplateText(Wt::WString::tr("mobile-search-title"));
	title->bindString("text", "Releases", Wt::PlainText);

	_releaseContainer = new Wt::WContainerWidget();
	wrapper->bindWidget("release-container", _releaseContainer);

	_showMore = new Wt::WTemplate();
	wrapper->bindWidget("show-more", _showMore);

	_showMore->setTemplateText(Wt::WString::tr("mobile-search-more"));
	_showMore->bindString("text", "Tap to show more results...");
	_showMore->hide();
	_showMore->clicked().connect(std::bind([=] {
		addResults(20);
	}));

	wApp->internalPathChanged().connect(std::bind([=] (std::string path)
	{
		const std::string pathPrefix = "/audio/release/";

		if (!wApp->internalPathMatches(pathPrefix))
			return;

		std::string strId = path.substr(pathPrefix.length());

		Release::id_type id;
		if (readAs(strId, id))
		{
			clear();
			search(SearchFilter::ById(SearchFilter::Field::Release, id), 20);
		}
	}, std::placeholders::_1));
}

void
ReleaseView::clear()
{
	// Flush the release container
	_releaseContainer->clear();

	// Flush the current context
	_currentTrackContainer = nullptr;
	_nbTracks = 0;
	_showMore->hide();
}

void
ReleaseView::search(SearchFilter filter, size_t nb)
{
	_filter = filter;

	clear();
	addResults(nb);
}

static
std::vector<Track::pointer >
getTracks(SearchFilter filter, size_t offset, size_t nb, bool &moreResults)
{
	std::vector<Track::pointer > tracks = Track::getByFilter(DboSession(), filter, offset, nb + 1);

	if (tracks.size() == nb + 1)
	{
		moreResults = true;
		tracks.pop_back();
	}
	else
		moreResults = false;

	return tracks;
}

static Wt::WString
getArtistNameFromRelease(Release::pointer release)
{
	auto artists = Artist::getByFilter(DboSession(),
			SearchFilter::ById(SearchFilter::Field::Release, release.id()), -1, 2);

	if (artists.size() > 1)
		return Wt::WString::fromUTF8("Various artists", Wt::PlainText);
	else
		return Wt::WString::fromUTF8(artists.front()->getName(), Wt::PlainText);
}

void
ReleaseView::addResults(size_t nb)
{
	Wt::Dbo::Transaction transaction(DboSession());

	bool moreResults;
	std::vector<Track::pointer > tracks = getTracks(_filter, _nbTracks, nb, moreResults);

	for (Track::pointer track : tracks)
	{
		// First check if we need to create a new track container

		// New container if it is the first one or if the release has changed
		if (!_currentTrackContainer
			|| _currentReleaseId != track->getRelease().id())
		{
			Release::pointer release = track->getRelease();

			Wt::WTemplate *releaseContainer = new Wt::WTemplate(_releaseContainer);
			releaseContainer->setTemplateText(Wt::WString::tr("wa-trackview-release-container"));
			_currentReleaseId = release.id();

			Wt::WImage *cover = new Wt::WImage();
			cover->setStyleClass ("center-block img-responsive"); // TODO move to CSS?
			cover->setImageLink(Wt::WLink (LmsApp->getImageResource()->getReleaseUrl(release.id(), 512)));

			releaseContainer->bindWidget("cover", cover);
			releaseContainer->bindString("artist-name", getArtistNameFromRelease(release), Wt::PlainText);
			releaseContainer->bindString("release-name", release->getName(), Wt::PlainText);
			int year = release->getReleaseYear();
			if (year > 0)
			{
				releaseContainer->setCondition("if-has-year", true);
				releaseContainer->bindInt("year", year);

				int originalYear = release->getReleaseYear(true);
				if (originalYear > 0 && originalYear != year)
				{
					releaseContainer->setCondition("if-has-orig-year", true);
					releaseContainer->bindInt("orig-year", originalYear);
				}
			}

			_currentTrackContainer = new Wt::WContainerWidget();
			releaseContainer->bindWidget("track-container", _currentTrackContainer);
		}

		assert(_currentTrackContainer != nullptr);

		Wt::WTemplate* trackRes = new Wt::WTemplate(_currentTrackContainer);
		trackRes->setTemplateText(Wt::WString::tr("wa-trackview-track"));

		if (track->getTrackNumber() > 0 && !track->getRelease()->isNone())
		{
			trackRes->setCondition("if-has-track-num", true);
	 		trackRes->bindInt("track-num", track->getTrackNumber());

			if (track->getDiscNumber() > 0 && track->getTotalDiscNumber() > 1)
			{	trackRes->setCondition("if-has-disc-num", true);
				trackRes->bindInt("disc-num", track->getDiscNumber());
			}
		}
 		trackRes->bindString("track-name", Wt::WString::fromUTF8(track->getName()), Wt::PlainText);
		// TODO, display artist name for compilation releases?

		std::string format = track->getDuration().total_seconds() < 3600 ? "%M:%S" : "%H:%M:%S";
		trackRes->bindString("time", durationToString(track->getDuration(), format), Wt::PlainText);

		Wt::WText *playBtn = new Wt::WText("Play", Wt::PlainText);
		playBtn->setStyleClass("center-block"); // TODO move to CSS?
		playBtn->clicked().connect(std::bind([=] {
			_sigTrackPlay.emit(track.id());
		}));

		trackRes->bindWidget("btn", playBtn);
		_nbTracks++;
	}

	if (moreResults)
		_showMore->show();
	else
		_showMore->hide();
}

Wt::WLink
ReleaseView::getLink(Database::Release::id_type id)
{
	return Wt::WLink(Wt::WLink::InternalPath, "/audio/release/" + std::to_string(id));
}

} // namespace Mobile
} // namespace UserInterface
