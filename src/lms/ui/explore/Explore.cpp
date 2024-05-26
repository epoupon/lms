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

#include "Explore.hpp"

#include <Wt/WApplication.h>
#include <Wt/WStackedWidget.h>

#include "ArtistView.hpp"
#include "ArtistsView.hpp"
#include "Filters.hpp"
#include "MultisearchView.hpp"
#include "ReleasesView.hpp"
#include "ReleaseView.hpp"
#include "TrackListView.hpp"
#include "TrackListsView.hpp"
#include "TracksView.hpp"

namespace lms::ui
{
    namespace
    {
        void handleContentsPathChange(Wt::WStackedWidget* stack)
        {
            enum Idx
            {
                IdxArtists = 0,
                IdxArtist,
                IdxTrackLists,
                IdxTrackList,
                IdxReleases,
                IdxRelease,
                IdxTracks,
                IdxMultisearch
            };

            static const std::map<std::string, int> indexes =
            {
                { "/artists",		IdxArtists },
                { "/artist",		IdxArtist },
                { "/tracklists",	IdxTrackLists },
                { "/tracklist",		IdxTrackList },
                { "/releases",		IdxReleases },
                { "/release",		IdxRelease },
                { "/tracks",		IdxTracks },
                { "/multisearch",   IdxMultisearch }
            };

            for (const auto& index : indexes)
            {
                if (wApp->internalPathMatches(index.first))
                {
                    stack->setCurrentIndex(index.second);
                    return;
                }
            }
        }

    } // namespace

    Explore::Explore(Filters& filters, PlayQueue& playQueue, Wt::WLineEdit& multisearchEdit)
        : Wt::WTemplate{ Wt::WString::tr("Lms.Explore.template") }
        , _playQueueController{ filters, playQueue }
    {
        addFunction("tr", &Functions::tr);

        // Contents
        Wt::WStackedWidget* contentsStack{ bindNew<Wt::WStackedWidget>("contents") };
        contentsStack->setOverflow(Wt::Overflow::Visible); // wt makes it hidden by default

        // same order as enum Idx
        auto artists = std::make_unique<Artists>(filters);
        contentsStack->addWidget(std::move(artists));

        auto artist = std::make_unique<Artist>(filters, _playQueueController);
        contentsStack->addWidget(std::move(artist));

        auto trackLists{ std::make_unique<TrackLists>(filters) };
        auto trackList{ std::make_unique<TrackList>(filters, _playQueueController) };
        trackList->trackListDeleted.connect(trackLists.get(), &TrackLists::onTrackListDeleted);
        contentsStack->addWidget(std::move(trackLists));
        contentsStack->addWidget(std::move(trackList));

        auto releases = std::make_unique<Releases>(filters, _playQueueController);
        contentsStack->addWidget(std::move(releases));

        auto release = std::make_unique<Release>(filters, _playQueueController);
        contentsStack->addWidget(std::move(release));

        auto tracks = std::make_unique<Tracks>(filters, _playQueueController);
        contentsStack->addWidget(std::move(tracks));

        auto multisearch = std::make_unique<Multisearch>(filters, _playQueueController, multisearchEdit);
        contentsStack->addWidget(std::move(multisearch));

        wApp->internalPathChanged().connect(this, [contentsStack]
            {
                handleContentsPathChange(contentsStack);
            });

        handleContentsPathChange(contentsStack);
    }
} // namespace lms::ui
