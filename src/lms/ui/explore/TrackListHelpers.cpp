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
#include <Wt/WText.h>

#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Track.hpp"
#include "resource/ImageResource.hpp"
#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"
#include "TrackStringUtils.hpp"

using namespace Database;

namespace UserInterface::TrackListHelpers
{
	std::unique_ptr<Wt::WTemplate>
	createEntry(const Wt::Dbo::ptr<Database::Track>& track, PlayQueueActionSignal& tracksAction)
	{
		auto entry {std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Tracks.template.entry"))};
		auto* entryPtr {entry.get()};

		Wt::WText* name {entry->bindNew<Wt::WText>("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain)};
		name->setToolTip(Wt::WString::fromUTF8(track->getName()));

		const auto artists {track->getArtists()};
		const Release::pointer release {track->getRelease()};
		const IdType trackId {track.id()};

		if (!artists.empty() || release)
			entry->setCondition("if-has-artists-or-release", true);

		if (!artists.empty())
		{
			entry->setCondition("if-has-artists", true);

			Wt::WContainerWidget* artistContainer {entry->bindNew<Wt::WContainerWidget>("artists")};
			for (const Artist::pointer& artist : artists)
			{
				Wt::WTemplate* a {artistContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Tracks.template.entry-artist"))};
				a->bindWidget("artist", LmsApplication::createArtistAnchor(artist));
			}
		}

		if (track->getRelease())
		{
			entry->setCondition("if-has-release", true);
			entry->bindWidget("release", LmsApplication::createReleaseAnchor(track->getRelease()));
			{
				Wt::WAnchor* anchor = entry->bindWidget("cover", LmsApplication::createReleaseAnchor(release, false));
				auto cover = std::make_unique<Wt::WImage>();
				cover->setImageLink(LmsApp->getImageResource()->getReleaseUrl(release.id(), ImageResource::Size::Large));
				cover->setStyleClass("Lms-cover");
				cover->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)");
				anchor->setImage(std::move(cover));
			}
		}
		else
		{
			auto cover = entry->bindNew<Wt::WImage>("cover");
			cover->setImageLink(LmsApp->getImageResource()->getTrackUrl(trackId, ImageResource::Size::Large));
			cover->setStyleClass("Lms-cover");
			cover->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)");
		}

		entry->bindString("duration", trackDurationToString(track->getDuration()), Wt::TextFormat::Plain);

		Wt::WText* playBtn = entry->bindNew<Wt::WText>("play-btn", Wt::WString::tr("Lms.Explore.template.play-btn"), Wt::TextFormat::XHTML);
		playBtn->clicked().connect([trackId, &tracksAction]
		{
			tracksAction.emit(PlayQueueAction::Play, {trackId});
		});

		Wt::WText* moreBtn = entry->bindNew<Wt::WText>("more-btn", Wt::WString::tr("Lms.Explore.template.more-btn"), Wt::TextFormat::XHTML);
		moreBtn->clicked().connect([=, &tracksAction]
		{
			Wt::WPopupMenu* popup {LmsApp->createPopupMenu()};

			popup->addItem(Wt::WString::tr("Lms.Explore.play-last"))
				->triggered().connect(moreBtn, [=, &tracksAction]
				{
					tracksAction.emit(PlayQueueAction::PlayLast, {trackId});
				});

			popup->popup(moreBtn);
		});

		LmsApp->getMediaPlayer().trackLoaded.connect(entryPtr, [=] (Database::IdType loadedTrackId)
		{
			entryPtr->bindString("is-playing", loadedTrackId == trackId ? "Lms-entry-playing" : "");
		});

		if (auto trackIdLoaded {LmsApp->getMediaPlayer().getTrackLoaded()})
		{
			entry->bindString("is-playing", *trackIdLoaded == trackId ? "Lms-entry-playing" : "");
		}
		else
			entry->bindString("is-playing", "");

		return entry;
	}

} // namespace UserInterface

