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
#include <Wt/WImage.h>
#include <Wt/WPushButton.h>

#include "av/IAudioFile.hpp"
#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/Types.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackLyrics.hpp"
#include "database/objects/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"
#include "ModalManager.hpp"
#include "Utils.hpp"
#include "common/Template.hpp"
#include "explore/PlayQueueController.hpp"
#include "resource/ArtworkResource.hpp"
#include "resource/DownloadResource.hpp"

namespace lms::ui::TrackListHelpers
{
    std::map<Wt::WString, std::set<db::ArtistId>> getArtistsByRole(db::TrackId trackId, core::EnumSet<db::TrackArtistLinkType> artistLinkTypes)
    {
        std::map<Wt::WString, std::set<db::ArtistId>> artistMap;

        auto addArtists = [&](db::TrackArtistLinkType linkType, const char* type) {
            if (!artistLinkTypes.contains(linkType))
                return;

            db::Artist::FindParameters params;
            params.setTrack(trackId);
            params.setLinkType(linkType);
            const auto artistIds{ db::Artist::findIds(LmsApp->getDbSession(), params) };
            if (artistIds.results.empty())
                return;

            Wt::WString typeStr{ Wt::WString::trn(type, artistIds.results.size()) };

            for (db::ArtistId artistId : artistIds.results)
                artistMap[typeStr].insert(artistId);
        };

        auto addPerformerArtists = [&] {
            if (!artistLinkTypes.contains(db::TrackArtistLinkType::Performer))
                return;

            db::TrackArtistLink::FindParameters params;
            params.setTrack(trackId);
            params.setLinkType(db::TrackArtistLinkType::Performer);

            db::TrackArtistLink::find(LmsApp->getDbSession(), params, [&](const db::TrackArtistLink::pointer& link) {
                artistMap[std::string{ link->getSubType() }].insert(link->getArtist()->getId());
            });
        };

        addArtists(db::TrackArtistLinkType::Composer, "Lms.Explore.Artists.linktype-composer");
        addArtists(db::TrackArtistLinkType::Conductor, "Lms.Explore.Artists.linktype-conductor");
        addArtists(db::TrackArtistLinkType::Lyricist, "Lms.Explore.Artists.linktype-lyricist");
        addArtists(db::TrackArtistLinkType::Mixer, "Lms.Explore.Artists.linktype-mixer");
        addArtists(db::TrackArtistLinkType::Remixer, "Lms.Explore.Artists.linktype-remixer");
        addArtists(db::TrackArtistLinkType::Producer, "Lms.Explore.Artists.linktype-producer");
        addPerformerArtists();

        if (auto itRolelessPerformers{ artistMap.find("") }; itRolelessPerformers != std::cend(artistMap))
        {
            Wt::WString performersStr{ Wt::WString::trn("Lms.Explore.Artists.linktype-performer", itRolelessPerformers->second.size()) };
            artistMap[performersStr] = std::move(itRolelessPerformers->second);
            artistMap.erase(itRolelessPerformers);
        }

        return artistMap;
    }

    void showTrackInfoModal(db::TrackId trackId, Filters& filters)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Track::pointer track{ db::Track::find(LmsApp->getDbSession(), trackId) };
        if (!track)
            return;

        auto trackInfo{ std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Tracks.template.track-info")) };
        Wt::WWidget* trackInfoPtr{ trackInfo.get() };
        trackInfo->addFunction("tr", &Wt::WTemplate::Functions::tr);

        std::map<Wt::WString, std::set<db::ArtistId>> artistMap{ getArtistsByRole(trackId) };
        if (!artistMap.empty())
        {
            trackInfo->setCondition("if-has-artist", true);
            Wt::WContainerWidget* artistTable{ trackInfo->bindNew<Wt::WContainerWidget>("artist-table") };

            for (const auto& [role, artistIds] : artistMap)
            {
                std::unique_ptr<Wt::WContainerWidget> artistContainer{ utils::createArtistAnchorList(std::vector(std::cbegin(artistIds), std::cend(artistIds))) };
                auto artistsEntry{ std::make_unique<Template>(Wt::WString::tr("Lms.Explore.template.info.artists")) };
                artistsEntry->bindString("type", role, Wt::TextFormat::Plain);
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
                trackInfo->bindString("codec", audioStream->codecName, Wt::TextFormat::Plain);
            }
        }

        trackInfo->bindString("duration", utils::durationToString(track->getDuration()));
        if (track->getBitrate())
        {
            trackInfo->setCondition("if-has-bitrate", true);
            trackInfo->bindString("bitrate", std::to_string(track->getBitrate() / 1000) + " kbps");
        }

        trackInfo->bindInt("playcount", core::Service<scrobbling::IScrobblingService>::get()->getCount(LmsApp->getUserId(), track->getId()));

        if (std::string_view comment{ track->getComment() }; !comment.empty())
        {
            trackInfo->setCondition("if-has-comment", true);
            trackInfo->bindString("comment", Wt::WString::fromUTF8(std::string{ comment }), Wt::TextFormat::Plain);
        }

        Wt::WContainerWidget* clusterContainer{ trackInfo->bindWidget("clusters", utils::createFilterClustersForTrack(track, filters)) };
        if (clusterContainer->count() > 0)
            trackInfo->setCondition("if-has-clusters", true);

        Wt::WPushButton* okBtn{ trackInfo->bindNew<Wt::WPushButton>("ok-btn", Wt::WString::tr("Lms.ok")) };
        okBtn->clicked().connect([=] {
            LmsApp->getModalManager().dispose(trackInfoPtr);
        });

        LmsApp->getModalManager().show(std::move(trackInfo));
    }

    void showTrackLyricsModal(db::TrackId trackId)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        auto trackLyrics{ std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Tracks.template.track-lyrics")) };
        Wt::WWidget* trackLyricsPtr{ trackLyrics.get() };
        trackLyrics->addFunction("tr", &Wt::WTemplate::Functions::tr);

        Wt::WContainerWidget* lyricsContainer{ trackLyrics->bindNew<Wt::WContainerWidget>("lyrics") };

        // limitation: display only first lyrics for now
        db::TrackLyrics::FindParameters params;
        params.setTrack(trackId);
        params.setSortMethod(db::TrackLyricsSortMethod::ExternalFirst);
        params.setRange(db::Range{ 0, 1 });

        db::TrackLyrics::find(LmsApp->getDbSession(), params, [&](const db::TrackLyrics::pointer& lyrics) {
            std::string text;
            auto addLine = [&](std::string_view line) {
                if (!text.empty())
                    text += '\n';
                text += line;
            };

            if (lyrics->isSynchronized())
            {
                for (const auto& [timestamp, line] : lyrics->getSynchronizedLines())
                    addLine(line);
            }
            else
            {
                for (const auto& line : lyrics->getUnsynchronizedLines())
                    addLine(line);
            }

            lyricsContainer->addNew<Wt::WText>(Wt::WString::fromUTF8(text), Wt::TextFormat::Plain);
        });

        Wt::WPushButton* okBtn{ trackLyrics->bindNew<Wt::WPushButton>("ok-btn", Wt::WString::tr("Lms.ok")) };
        okBtn->clicked().connect([=] {
            LmsApp->getModalManager().dispose(trackLyricsPtr);
        });

        LmsApp->getModalManager().show(std::move(trackLyrics));
    }

    std::unique_ptr<Wt::WWidget> createEntry(const db::ObjectPtr<db::Track>& track, PlayQueueController& playQueueController, Filters& filters)
    {
        auto entry{ std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Tracks.template.entry")) };
        auto* entryPtr{ entry.get() };

        entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

        const db::Release::pointer release{ track->getRelease() };
        const db::TrackId trackId{ track->getId() };

        const auto artists{ track->getArtistIds({ db::TrackArtistLinkType::Artist }) };
        if (!artists.empty())
        {
            entry->setCondition("if-has-artists", true);
            entry->bindWidget("artists", utils::createArtistDisplayNameWithAnchors(track->getArtistDisplayName(), artists));
            entry->bindWidget("artists-md", utils::createArtistDisplayNameWithAnchors(track->getArtistDisplayName(), artists));
        }

        std::unique_ptr<Wt::WImage> image;
        if (track->getPreferredMediaArtworkId().isValid())
            image = utils::createArtworkImage(track->getPreferredMediaArtworkId(), ArtworkResource::DefaultArtworkType::Track, ArtworkResource::Size::Small);
        else if (track->getPreferredArtworkId().isValid())
            image = utils::createArtworkImage(track->getPreferredArtworkId(), ArtworkResource::DefaultArtworkType::Track, ArtworkResource::Size::Small);
        else
            image = utils::createDefaultArtworkImage(ArtworkResource::DefaultArtworkType::Track);

        image->addStyleClass("Lms-cover-track rounded"); // hack
        if (track->getRelease())
        {
            entry->setCondition("if-has-release", true);
            entry->bindWidget("release", utils::createReleaseAnchor(track->getRelease()));

            Wt::WAnchor* anchor{ entry->bindWidget("cover", utils::createReleaseAnchor(release, false)) };
            image->addStyleClass("Lms-cover-anchor"); // HACK
            anchor->setImage(std::move((image)));
        }
        else
        {
            entry->bindWidget<Wt::WImage>("cover", std::move(image));
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

            Wt::WPushButton* starBtn{ entry->bindNew<Wt::WPushButton>("star-btn", Wt::WString::tr(isStarred() ? "Lms.template.unstar-btn" : "Lms.template.star-btn"), Wt::TextFormat::XHTML) };
            Wt::WPushButton* starMenuEntry{ entry->bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star")) };

            auto toggle{ [=] {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                if (isStarred())
                {
                    core::Service<feedback::IFeedbackService>::get()->unstar(LmsApp->getUserId(), trackId);
                    starMenuEntry->setText(Wt::WString::tr("Lms.Explore.star"));
                    starBtn->setText(Wt::WString::tr("Lms.template.star-btn"));
                }
                else
                {
                    core::Service<feedback::IFeedbackService>::get()->star(LmsApp->getUserId(), trackId);
                    starMenuEntry->setText(Wt::WString::tr("Lms.Explore.unstar"));
                    starBtn->setText(Wt::WString::tr("Lms.template.unstar-btn"));
                }
            } };

            starMenuEntry->clicked().connect([=] { toggle(); });
            starBtn->clicked().connect([=] { toggle(); });
        }

        entry->bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
            ->setLink(Wt::WLink{ std::make_unique<DownloadTrackResource>(trackId) });

        entry->bindNew<Wt::WPushButton>("track-info", Wt::WString::tr("Lms.Explore.track-info"))
            ->clicked()
            .connect([trackId, &filters] { showTrackInfoModal(trackId, filters); });

        if (track->hasLyrics())
        {
            entry->setCondition("if-has-lyrics", true);
            entry->bindNew<Wt::WPushButton>("track-lyrics", Wt::WString::tr("Lms.Explore.track-lyrics"))
                ->clicked()
                .connect([trackId] { showTrackLyricsModal(trackId); });
        }

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
