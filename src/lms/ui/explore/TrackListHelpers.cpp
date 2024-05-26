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

#include <map>

#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WPushButton.h>

#include "av/IAudioFile.hpp"
#include "core/ILogger.hpp"
#include "core/Service.hpp"
#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackArtistLink.hpp"
#include "database/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"
#include "ModalManager.hpp"
#include "Utils.hpp"
#include "common/Template.hpp"
#include "explore/PlayQueueController.hpp"
#include "resource/CoverResource.hpp"
#include "resource/DownloadResource.hpp"

namespace lms::ui::TrackListHelpers
{
    using namespace db;

    void showTrackInfoModal(db::TrackId trackId, Filters& filters)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Track::pointer track{ Track::find(LmsApp->getDbSession(), trackId) };
        if (!track)
            return;

        auto trackInfo{ std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Tracks.template.track-info")) };
        Wt::WWidget* trackInfoPtr{ trackInfo.get() };
        trackInfo->addFunction("tr", &Wt::WTemplate::Functions::tr);

        std::map<Wt::WString, std::set<ArtistId>> artistMap;

        auto addArtists = [&](TrackArtistLinkType linkType, const char* type) {
            Artist::FindParameters params;
            params.setTrack(trackId);
            params.setLinkType(linkType);
            const auto artistIds{ Artist::findIds(LmsApp->getDbSession(), params) };
            if (artistIds.results.empty())
                return;

            Wt::WString typeStr{ Wt::WString::trn(type, artistIds.results.size()) };
            ;
            for (ArtistId artistId : artistIds.results)
                artistMap[typeStr].insert(artistId);
        };

        auto addPerformerArtists = [&] {
            TrackArtistLink::FindParameters params;
            params.setTrack(trackId);
            params.setLinkType(TrackArtistLinkType::Performer);

            TrackArtistLink::find(LmsApp->getDbSession(), params, [&](const TrackArtistLink::pointer& link) {
                artistMap[std::string{ link->getSubType() }].insert(link->getArtist()->getId());
            });
        };

        addArtists(TrackArtistLinkType::Composer, "Lms.Explore.Artists.linktype-composer");
        addArtists(TrackArtistLinkType::Conductor, "Lms.Explore.Artists.linktype-conductor");
        addArtists(TrackArtistLinkType::Lyricist, "Lms.Explore.Artists.linktype-lyricist");
        addArtists(TrackArtistLinkType::Mixer, "Lms.Explore.Artists.linktype-mixer");
        addArtists(TrackArtistLinkType::Remixer, "Lms.Explore.Artists.linktype-remixer");
        addArtists(TrackArtistLinkType::Producer, "Lms.Explore.Artists.linktype-producer");
        addPerformerArtists();

        if (auto itRolelessPerformers{ artistMap.find("") }; itRolelessPerformers != std::cend(artistMap))
        {
            Wt::WString performersStr{ Wt::WString::trn("Lms.Explore.Artists.linktype-performer", itRolelessPerformers->second.size()) };
            artistMap[performersStr] = std::move(itRolelessPerformers->second);
            artistMap.erase(itRolelessPerformers);
        }

        if (!artistMap.empty())
        {
            trackInfo->setCondition("if-has-artist", true);
            Wt::WContainerWidget* artistTable{ trackInfo->bindNew<Wt::WContainerWidget>("artist-table") };

            for (const auto& [role, artistIds] : artistMap)
            {
                std::unique_ptr<Wt::WContainerWidget> artistContainer{ utils::createArtistAnchorList(std::vector(std::cbegin(artistIds), std::cend(artistIds))) };
                auto artistsEntry{ std::make_unique<Template>(Wt::WString::tr("Lms.Explore.template.info.artists")) };
                artistsEntry->bindString("type", role);
                artistsEntry->bindWidget("artist-container", std::move(artistContainer));
                artistTable->addWidget(std::move(artistsEntry));
            }
        }

        if (const auto audioFile{ av::parseAudioFile(track->getAbsoluteFilePath()) })
        {
            const std::optional<av::StreamInfo> audioStream{ audioFile->getBestStreamInfo() };
            if (audioStream)
            {
                trackInfo->setCondition("if-has-codec", true);
                trackInfo->bindString("codec", audioStream->codecName);
            }
        }

        trackInfo->bindString("duration", utils::durationToString(track->getDuration()));
        if (track->getBitrate())
        {
            trackInfo->setCondition("if-has-bitrate", true);
            trackInfo->bindString("bitrate", std::to_string(track->getBitrate() / 1000) + " kbps");
        }

        trackInfo->bindInt("playcount", core::Service<scrobbling::IScrobblingService>::get()->getCount(LmsApp->getUserId(), track->getId()));

        Wt::WContainerWidget* clusterContainer{ trackInfo->bindWidget("clusters", utils::createFilterClustersForTrack(track, filters)) };
        if (clusterContainer->count() > 0)
            trackInfo->setCondition("if-has-clusters", true);

        Wt::WPushButton* okBtn{ trackInfo->bindNew<Wt::WPushButton>("ok-btn", Wt::WString::tr("Lms.ok")) };
        okBtn->clicked().connect([=] {
            LmsApp->getModalManager().dispose(trackInfoPtr);
        });

        LmsApp->getModalManager().show(std::move(trackInfo));
    }

    std::unique_ptr<Wt::WWidget> createEntry(const db::ObjectPtr<db::Track>& track, PlayQueueController& playQueueController, Filters& filters)
    {
        auto entry{ std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Tracks.template.entry")) };
        auto* entryPtr{ entry.get() };

        entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

        const Release::pointer release{ track->getRelease() };
        const TrackId trackId{ track->getId() };

        const auto artists{ track->getArtistIds({ TrackArtistLinkType::Artist }) };
        if (!artists.empty())
        {
            entry->setCondition("if-has-artists", true);
            entry->bindWidget("artists", utils::createArtistDisplayNameWithAnchors(track->getArtistDisplayName(), artists));
            entry->bindWidget("artists-md", utils::createArtistDisplayNameWithAnchors(track->getArtistDisplayName(), artists));
        }

        if (track->getRelease())
        {
            entry->setCondition("if-has-release", true);
            entry->bindWidget("release", utils::createReleaseAnchor(track->getRelease()));
            Wt::WAnchor* anchor{ entry->bindWidget("cover", utils::createReleaseAnchor(release, false)) };
            auto cover{ utils::createCover(release->getId(), CoverResource::Size::Small) };
            cover->addStyleClass("Lms-cover-track Lms-cover-anchor"); // HACK
            anchor->setImage(std::move((cover)));
        }
        else
        {
            auto cover{ utils::createCover(trackId, CoverResource::Size::Small) };
            cover->addStyleClass("Lms-cover-track"); // HACK
            entry->bindWidget<Wt::WImage>("cover", std::move(cover));
        }

        entry->bindString("duration", utils::durationToString(track->getDuration()), Wt::TextFormat::Plain);

        Wt::WPushButton* playBtn{ entry->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.template.play-btn"), Wt::TextFormat::XHTML) };
        playBtn->clicked().connect([trackId, &playQueueController] {
            playQueueController.processCommand(PlayQueueController::Command::Play, { trackId });
        });

        entry->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.template.more-btn"), Wt::TextFormat::XHTML);
        entry->bindNew<Wt::WPushButton>("play", Wt::WString::tr("Lms.Explore.play"))
            ->clicked()
            .connect([trackId, &playQueueController] {
                playQueueController.processCommand(PlayQueueController::Command::Play, { trackId });
            });
        entry->bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"))
            ->clicked()
            .connect([=, &playQueueController] {
                playQueueController.processCommand(PlayQueueController::Command::PlayNext, { trackId });
            });
        entry->bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"))
            ->clicked()
            .connect([=, &playQueueController] {
                playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, { trackId });
            });

        {
            auto isStarred{ [=] { return core::Service<feedback::IFeedbackService>::get()->isStarred(LmsApp->getUserId(), trackId); } };

            Wt::WPushButton* starBtn{ entry->bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star")) };
            starBtn->clicked().connect([=] {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                if (isStarred())
                {
                    core::Service<feedback::IFeedbackService>::get()->unstar(LmsApp->getUserId(), trackId);
                    starBtn->setText(Wt::WString::tr("Lms.Explore.star"));
                }
                else
                {
                    core::Service<feedback::IFeedbackService>::get()->star(LmsApp->getUserId(), trackId);
                    starBtn->setText(Wt::WString::tr("Lms.Explore.unstar"));
                }
            });
        }

        entry->bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
            ->setLink(Wt::WLink{ std::make_unique<DownloadTrackResource>(trackId) });

        entry->bindNew<Wt::WPushButton>("track-info", Wt::WString::tr("Lms.Explore.track-info"))
            ->clicked()
            .connect([trackId, &filters] { showTrackInfoModal(trackId, filters); });

        LmsApp->getMediaPlayer().trackLoaded.connect(entryPtr, [=](db::TrackId loadedTrackId) {
            entryPtr->toggleStyleClass("Lms-entry-playing", loadedTrackId == trackId);
        });

        if (auto trackIdLoaded{ LmsApp->getMediaPlayer().getTrackLoaded() })
        {
            entryPtr->toggleStyleClass("Lms-entry-playing", *trackIdLoaded == trackId);
        }
        else
            entry->removeStyleClass("Lms-entry-playing");

        return entry;
    }

} // namespace lms::ui::TrackListHelpers
