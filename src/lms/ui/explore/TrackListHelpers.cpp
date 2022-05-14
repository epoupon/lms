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

#include "TrackListHelpers.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WImage.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"

#include "common/Template.hpp"
#include "resource/DownloadResource.hpp"
#include "resource/CoverResource.hpp"
#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"
#include "TrackStringUtils.hpp"

using namespace Database;

namespace UserInterface::TrackListHelpers
{
	std::unique_ptr<Wt::WWidget>
	createEntry(const Database::ObjectPtr<Database::Track>& track, PlayQueueActionTrackSignal& tracksAction)
	{
		auto entry {std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Tracks.template.entry"))};
		auto* entryPtr {entry.get()};

		entry->bindString("name", Wt::WString::fromUTF8(track->getName()));

		const auto artists {track->getArtists({TrackArtistLinkType::Artist})};
		const Release::pointer release {track->getRelease()};
		const TrackId trackId {track->getId()};

		if (track->getRelease())
		{
			entry->setCondition("if-has-release", true);
			entry->bindWidget("release", LmsApplication::createReleaseAnchor(track->getRelease()));
			{
				Wt::WAnchor* anchor {entry->bindWidget("cover", LmsApplication::createReleaseAnchor(release, false))};
				auto cover {std::make_unique<Wt::WImage>()};
				cover->setImageLink(LmsApp->getCoverResource()->getReleaseUrl(release->getId(), CoverResource::Size::Large));
				cover->setStyleClass("img-fluid");
				cover->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)");
				anchor->setImage(std::move(cover));
			}
		}
		else
		{
			auto* cover {entry->bindNew<Wt::WImage>("cover")};
			cover->setImageLink(LmsApp->getCoverResource()->getTrackUrl(trackId, CoverResource::Size::Large));
			cover->setStyleClass("img-fluid");
			cover->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)");
		}

		entry->bindString("duration", durationToString(track->getDuration()), Wt::TextFormat::Plain);

		Wt::WText* playBtn = entry->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML);
		playBtn->clicked().connect([trackId, &tracksAction]
		{
			tracksAction.emit(PlayQueueAction::Play, {trackId});
		});

		entry->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.Explore.template.more-btn"), Wt::TextFormat::XHTML);
		entry->bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"))
			->clicked().connect([=, &tracksAction]
			{
				tracksAction.emit(PlayQueueAction::PlayLast, {trackId});
			});

		{
			auto isStarred {[=] { return Service<Scrobbling::IScrobblingService>::get()->isStarred(LmsApp->getUserId(), trackId); }};

			Wt::WPushButton* starBtn {entry->bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star"))};
			starBtn->clicked().connect([=]
			{
				auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

				if (isStarred())
				{
					Service<Scrobbling::IScrobblingService>::get()->unstar(LmsApp->getUserId(), trackId);
					starBtn->setText(Wt::WString::tr("Lms.Explore.star"));
				}
				else
				{
					Service<Scrobbling::IScrobblingService>::get()->star(LmsApp->getUserId(), trackId);
					starBtn->setText(Wt::WString::tr("Lms.Explore.unstar"));
				}
			});

		}

		entry->bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
			->setLink(Wt::WLink {std::make_unique<DownloadTrackResource>(trackId)});

		LmsApp->getMediaPlayer().trackLoaded.connect(entryPtr, [=] (Database::TrackId loadedTrackId)
		{
			entryPtr->toggleStyleClass("Lms-entry-playing", loadedTrackId == trackId);
		});

		if (auto trackIdLoaded {LmsApp->getMediaPlayer().getTrackLoaded()})
		{
			entryPtr->toggleStyleClass("Lms-entry-playing", *trackIdLoaded == trackId);
		}
		else
			entry->removeStyleClass("Lms-entry-playing");

		return entry;
	}

} // namespace UserInterface

