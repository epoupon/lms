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

#include "ReleaseView.hpp"

#include <map>

#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WPushButton.h>

#include "av/IAudioFile.hpp"
#include "core/ILogger.hpp"
#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Release.hpp"
#include "database/ScanSettings.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackArtistLink.hpp"
#include "database/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/recommendation/IRecommendationService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "LmsApplication.hpp"
#include "LmsApplicationException.hpp"
#include "MediaPlayer.hpp"
#include "ModalManager.hpp"
#include "Utils.hpp"
#include "common/Template.hpp"
#include "explore/Filters.hpp"
#include "explore/PlayQueueController.hpp"
#include "explore/ReleaseHelpers.hpp"
#include "explore/TrackListHelpers.hpp"
#include "resource/DownloadResource.hpp"

namespace lms::ui
{
    using namespace db;

    namespace
    {
        void showReleaseInfoModal(db::ReleaseId releaseId)
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const db::Release::pointer release{ db::Release::find(LmsApp->getDbSession(), releaseId) };
            if (!release)
                return;

            auto releaseInfo{ std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Release.template.release-info")) };
            Wt::WWidget* releaseInfoPtr{ releaseInfo.get() };
            releaseInfo->addFunction("tr", &Wt::WTemplate::Functions::tr);

            if (const auto releaseTypeNames{ release->getReleaseTypeNames() }; !releaseTypeNames.empty())
            {
                releaseInfo->setCondition("if-has-release-type", true);
                releaseInfo->bindString("release-type", releaseHelpers::buildReleaseTypeString(parseReleaseType(releaseTypeNames)));
            }

            std::map<Wt::WString, std::set<ArtistId>> artistMap;

            auto addArtists = [&](TrackArtistLinkType linkType, const char* type) {
                Artist::FindParameters params;
                params.setRelease(releaseId);
                params.setLinkType(linkType);
                const auto artistIds{ Artist::findIds(LmsApp->getDbSession(), params) };
                if (artistIds.results.empty())
                    return;

                Wt::WString typeStr{ Wt::WString::trn(type, artistIds.results.size()) };
                for (ArtistId artistId : artistIds.results)
                    artistMap[typeStr].insert(artistId);
            };

            auto addPerformerArtists = [&] {
                TrackArtistLink::FindParameters params;
                params.setRelease(releaseId);
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
                releaseInfo->setCondition("if-has-artist", true);
                Wt::WContainerWidget* artistTable{ releaseInfo->bindNew<Wt::WContainerWidget>("artist-table") };

                for (const auto& [role, artistIds] : artistMap)
                {
                    std::unique_ptr<Wt::WContainerWidget> artistContainer{ utils::createArtistAnchorList(std::vector(std::cbegin(artistIds), std::cend(artistIds))) };
                    auto artistsEntry{ std::make_unique<Template>(Wt::WString::tr("Lms.Explore.template.info.artists")) };
                    artistsEntry->bindString("type", role);
                    artistsEntry->bindWidget("artist-container", std::move(artistContainer));
                    artistTable->addWidget(std::move(artistsEntry));
                }
            }

            // TODO: save in DB and aggregate all this
            for (const Track::pointer& track : Track::find(LmsApp->getDbSession(), Track::FindParameters{}.setRelease(releaseId).setRange(Range{ 0, 1 })).results)
            {
                if (const auto audioFile{ av::parseAudioFile(track->getAbsoluteFilePath()) })
                {
                    const std::optional<av::StreamInfo> audioStream{ audioFile->getBestStreamInfo() };
                    if (audioStream)
                    {
                        releaseInfo->setCondition("if-has-codec", true);
                        releaseInfo->bindString("codec", audioStream->codecName);
                        break;
                    }
                }
            }

            if (const std::size_t meanBitrate{ release->getMeanBitrate() })
            {
                releaseInfo->setCondition("if-has-bitrate", true);
                releaseInfo->bindString("bitrate", std::to_string(meanBitrate / 1000) + " kbps");
            }

            releaseInfo->bindInt("playcount", core::Service<scrobbling::IScrobblingService>::get()->getCount(LmsApp->getUserId(), release->getId()));

            Wt::WPushButton* okBtn{ releaseInfo->bindNew<Wt::WPushButton>("ok-btn", Wt::WString::tr("Lms.ok")) };
            okBtn->clicked().connect([=] {
                LmsApp->getModalManager().dispose(releaseInfoPtr);
            });

            LmsApp->getModalManager().show(std::move(releaseInfo));
        }

        std::optional<ReleaseId> extractReleaseIdFromInternalPath()
        {
            if (wApp->internalPathMatches("/release/mbid/"))
            {
                const auto mbid{ core::UUID::fromString(wApp->internalPathNextPart("/release/mbid/")) };
                if (mbid)
                {
                    auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                    if (const db::Release::pointer release{ db::Release::find(LmsApp->getDbSession(), *mbid) })
                        return release->getId();
                }

                return std::nullopt;
            }

            return core::stringUtils::readAs<ReleaseId::ValueType>(wApp->internalPathNextPart("/release/"));
        }
    } // namespace

    Release::Release(Filters& filters, PlayQueueController& playQueueController)
        : Template{ Wt::WString::tr("Lms.Explore.Release.template") }
        , _filters{ filters }
        , _playQueueController{ playQueueController }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);
        addFunction("id", &Wt::WTemplate::Functions::id);

        wApp->internalPathChanged().connect(this, [this] {
            refreshView();
        });

        _filters.updated().connect([this] {
            _needForceRefresh = true;
            refreshView();
        });

        refreshView();
    }

    void Release::refreshView()
    {
        if (!wApp->internalPathMatches("/release/"))
            return;

        const auto releaseId{ extractReleaseIdFromInternalPath() };

        // consider everything is up to date is the same release is being rendered
        if (!_needForceRefresh && releaseId && *releaseId == _releaseId)
            return;

        clear();
        _releaseId = {};
        _needForceRefresh = false;

        if (!releaseId)
            throw ReleaseNotFoundException{};

        auto similarReleasesIds{ core::Service<recommendation::IRecommendationService>::get()->getSimilarReleases(*releaseId, 6) };

        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Release::pointer release{ db::Release::find(LmsApp->getDbSession(), *releaseId) };
        if (!release)
            throw ReleaseNotFoundException{};

        LmsApp->setTitle(std::string{ release->getName() });
        _releaseId = *releaseId;

        refreshCopyright(release);
        refreshLinks(release);
        refreshSimilarReleases(similarReleasesIds);

        bindString("name", Wt::WString::fromUTF8(std::string{ release->getName() }), Wt::TextFormat::Plain);

        Wt::WString year{ releaseHelpers::buildReleaseYearString(release->getYear(), release->getOriginalYear()) };
        if (!year.empty())
        {
            setCondition("if-has-year", true);
            bindString("year", year, Wt::TextFormat::Plain);
        }

        bindString("duration", utils::durationToString(release->getDuration()), Wt::TextFormat::Plain);

        refreshReleaseArtists(release);

        bindWidget<Wt::WImage>("cover", utils::createReleaseCover(release->getId(), ArtworkResource::Size::Large));

        Wt::WContainerWidget* clusterContainers{ bindNew<Wt::WContainerWidget>("clusters") };
        {
            const auto clusterTypeIds{ ClusterType::findIds(LmsApp->getDbSession()).results };
            const auto clusterGroups{ release->getClusterGroups(clusterTypeIds, 3) };

            for (const auto& clusters : clusterGroups)
            {
                for (const db::Cluster::pointer& cluster : clusters)
                {
                    const ClusterId clusterId{ cluster->getId() };
                    Wt::WInteractWidget* entry{ clusterContainers->addWidget(utils::createFilterCluster(clusterId)) };
                    entry->clicked().connect([this, clusterId] {
                        _filters.add(clusterId);
                    });
                }
            }
        }

        bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.play"), Wt::TextFormat::XHTML)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::Play, { _releaseId });
            });

        bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, { _releaseId });
            });

        bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayNext, { _releaseId });
            });

        bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, { _releaseId });
            });

        bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
            ->setLink(Wt::WLink{ std::make_unique<DownloadReleaseResource>(_releaseId) });

        bindNew<Wt::WPushButton>("release-info", Wt::WString::tr("Lms.Explore.release-info"))
            ->clicked()
            .connect([this] {
                showReleaseInfoModal(_releaseId);
            });

        {
            auto isStarred{ [this] { return core::Service<feedback::IFeedbackService>::get()->isStarred(LmsApp->getUserId(), _releaseId); } };

            Wt::WPushButton* starBtn{ bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star")) };
            starBtn->clicked().connect([=, this] {
                if (isStarred())
                {
                    core::Service<feedback::IFeedbackService>::get()->unstar(LmsApp->getUserId(), _releaseId);
                    starBtn->setText(Wt::WString::tr("Lms.Explore.star"));
                }
                else
                {
                    core::Service<feedback::IFeedbackService>::get()->star(LmsApp->getUserId(), _releaseId);
                    starBtn->setText(Wt::WString::tr("Lms.Explore.unstar"));
                }
            });
        }

        Wt::WContainerWidget* rootContainer{ bindNew<Wt::WContainerWidget>("container") };

        const bool variousArtists{ release->hasVariousArtists() };
        const auto totalDisc{ release->getTotalDisc() };
        const std::size_t discCount{ release->getDiscCount() };
        const bool hasDiscSubtitle{ release->hasDiscSubtitle() };
        const bool useSubtitleContainers{ (discCount > 1) || (totalDisc && *totalDisc > 1) || hasDiscSubtitle };

        // Expect to be called in asc order
        std::map<std::size_t, Wt::WContainerWidget*> trackContainers;
        auto getOrAddDiscContainer = [&, releaseId = _releaseId](std::size_t discNumber, const std::string& discSubtitle) -> Wt::WContainerWidget* {
            if (auto it{ trackContainers.find(discNumber) }; it != std::cend(trackContainers))
                return it->second;

            Template* disc{ rootContainer->addNew<Template>(Wt::WString::tr("Lms.Explore.Release.template.entry-disc")) };
            disc->addFunction("id", &Wt::WTemplate::Functions::id);

            if (discSubtitle.empty())
                disc->bindNew<Wt::WText>("disc-title", Wt::WString::tr("Lms.Explore.Release.disc").arg(discNumber));
            else
                disc->bindString("disc-title", Wt::WString::fromUTF8(discSubtitle), Wt::TextFormat::Plain);

            Wt::WPushButton* playBtn{ disc->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.template.play-btn"), Wt::TextFormat::XHTML) };
            playBtn->clicked().connect([this, releaseId, discNumber] {
                _playQueueController.processCommand(PlayQueueController::Command::Play, { PlayQueueController::Disc{ releaseId, discNumber } });
            });
            disc->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.template.more-btn"), Wt::TextFormat::XHTML);
            disc->bindNew<Wt::WPushButton>("play", Wt::WString::tr("Lms.Explore.play"))
                ->clicked()
                .connect([this, releaseId, discNumber] {
                    _playQueueController.processCommand(PlayQueueController::Command::Play, { PlayQueueController::Disc{ releaseId, discNumber } });
                });
            disc->bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"))
                ->clicked()
                .connect([this, releaseId, discNumber] {
                    _playQueueController.processCommand(PlayQueueController::Command::PlayNext, { PlayQueueController::Disc{ releaseId, discNumber } });
                });

            disc->bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
                ->clicked()
                .connect([this, releaseId, discNumber] {
                    _playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, { PlayQueueController::Disc{ releaseId, discNumber } });
                });
            disc->bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"))
                ->clicked()
                .connect([this, releaseId, discNumber] {
                    _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, { PlayQueueController::Disc{ releaseId, discNumber } });
                });

            Wt::WContainerWidget* tracksContainer{ disc->bindNew<Wt::WContainerWidget>("tracks") };
            trackContainers[discNumber] = tracksContainer;

            return tracksContainer;
        };

        Wt::WContainerWidget* noDiscTracksContainer{};
        auto getOrAddNoDiscContainer = [&] {
            if (noDiscTracksContainer)
                return noDiscTracksContainer;

            Wt::WTemplate* disc{ rootContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.entry-nodisc")) };
            noDiscTracksContainer = disc->bindNew<Wt::WContainerWidget>("tracks");

            return noDiscTracksContainer;
        };

        db::Track::FindParameters params;
        params.setRelease(_releaseId);
        params.setSortMethod(db::TrackSortMethod::Release);
        params.setClusters(_filters.getClusters());
        params.setMediaLibrary(_filters.getMediaLibrary());

        db::Track::find(LmsApp->getDbSession(), params, [&](const db::Track::pointer& track) {
            const db::TrackId trackId{ track->getId() };
            const auto discNumber{ track->getDiscNumber() };

            Wt::WContainerWidget* container;
            if (useSubtitleContainers && discNumber)
                container = getOrAddDiscContainer(*discNumber, track->getDiscSubtitle());
            else if (hasDiscSubtitle && !discNumber)
                container = getOrAddDiscContainer(0, track->getDiscSubtitle());
            else
                container = getOrAddNoDiscContainer();

            Template* entry{ container->addNew<Template>(Wt::WString::tr("Lms.Explore.Release.template.entry")) };
            entry->addFunction("id", &Wt::WTemplate::Functions::id);

            entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

            const auto artists{ track->getArtistIds({ TrackArtistLinkType::Artist }) };
            // TODO: display artist if it is single and not the one of the release (variousArtists is false in that case)
            if (variousArtists && !artists.empty())
            {
                entry->setCondition("if-has-artists", true);
                entry->bindWidget("artists", utils::createArtistDisplayNameWithAnchors(track->getArtistDisplayName(), artists));
                entry->bindWidget("artists-md", utils::createArtistDisplayNameWithAnchors(track->getArtistDisplayName(), artists));
            }

            auto trackNumber{ track->getTrackNumber() };
            if (trackNumber)
            {
                entry->setCondition("if-has-track-number", true);
                entry->bindInt("track-number", *trackNumber);
            }

            Wt::WPushButton* playBtn{ entry->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.template.play-btn"), Wt::TextFormat::XHTML) };
            playBtn->clicked().connect([this, trackId] {
                _playQueueController.playTrackInRelease(trackId);
            });

            {
                entry->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.template.more-btn"), Wt::TextFormat::XHTML);
                entry->bindNew<Wt::WPushButton>("play", Wt::WString::tr("Lms.Explore.play"))
                    ->clicked()
                    .connect([this, trackId] {
                        _playQueueController.playTrackInRelease(trackId);
                    });
                entry->bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"))
                    ->clicked()
                    .connect([this, trackId] {
                        _playQueueController.processCommand(PlayQueueController::Command::PlayNext, { trackId });
                    });
                entry->bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"))
                    ->clicked()
                    .connect([this, trackId] {
                        _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, { trackId });
                    });

                auto isStarred{ [=] { return core::Service<feedback::IFeedbackService>::get()->isStarred(LmsApp->getUserId(), trackId); } };

                Wt::WPushButton* starBtn{ entry->bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star")) };
                starBtn->clicked().connect([=] {
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

                entry->bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
                    ->setLink(Wt::WLink{ std::make_unique<DownloadTrackResource>(trackId) });

                entry->bindNew<Wt::WPushButton>("track-info", Wt::WString::tr("Lms.Explore.track-info"))
                    ->clicked()
                    .connect([this, trackId] { TrackListHelpers::showTrackInfoModal(trackId, _filters); });

                if (track->hasLyrics())
                {
                    entry->setCondition("if-has-lyrics", true);
                    entry->bindNew<Wt::WPushButton>("track-lyrics", Wt::WString::tr("Lms.Explore.track-lyrics"))
                        ->clicked()
                        .connect([trackId] { TrackListHelpers::showTrackLyricsModal(trackId); });
                }
            }

            entry->bindString("duration", utils::durationToString(track->getDuration()), Wt::TextFormat::Plain);

            LmsApp->getMediaPlayer().trackLoaded.connect(entry, [=](TrackId loadedTrackId) {
                entry->toggleStyleClass("Lms-entry-playing", loadedTrackId == trackId);
            });

            if (auto trackIdLoaded{ LmsApp->getMediaPlayer().getTrackLoaded() })
            {
                entry->toggleStyleClass("Lms-entry-playing", *trackIdLoaded == trackId);
            }
            else
                entry->removeStyleClass("Lms-entry-playing");
        });
    }

    void Release::refreshReleaseArtists(const db::Release::pointer& release)
    {
        auto container{ utils::createArtistsAnchorsForRelease(release) };
        if (container)
        {
            setCondition("if-has-release-artists", true);
            bindWidget("artists", std::move(container));
        }
    }

    void Release::refreshCopyright(const db::Release::pointer& release)
    {
        std::optional<std::string> copyright{ release->getCopyright() };
        std::optional<std::string> copyrightURL{ release->getCopyrightURL() };

        if (!copyright && !copyrightURL)
            return;

        setCondition("if-has-copyright", true);

        std::string copyrightText{ copyright ? *copyright : "" };
        if (copyrightText.empty() && copyrightURL)
            copyrightText = *copyrightURL;

        if (copyrightURL)
        {
            Wt::WLink link{ *copyrightURL };
            link.setTarget(Wt::LinkTarget::NewWindow);

            Wt::WAnchor* anchor{ bindNew<Wt::WAnchor>("copyright", link) };
            anchor->setTextFormat(Wt::TextFormat::Plain);
            anchor->setText(Wt::WString::fromUTF8(copyrightText));
        }
        else
            bindString("copyright", Wt::WString::fromUTF8(*copyright), Wt::TextFormat::Plain);
    }

    void Release::refreshLinks(const db::Release::pointer& release)
    {
        const auto mbid{ release->getMBID() };
        if (mbid)
        {
            setCondition("if-has-mbid", true);
            bindString("mbid-link", std::string{ "https://musicbrainz.org/release/" } + std::string{ mbid->getAsString() });
        }
    }

    void Release::refreshSimilarReleases(const std::vector<ReleaseId>& similarReleasesId)
    {
        if (similarReleasesId.empty())
            return;

        setCondition("if-has-similar-releases", true);
        auto* similarReleasesContainer{ bindNew<Wt::WContainerWidget>("similar-releases") };

        for (const ReleaseId id : similarReleasesId)
        {
            const db::Release::pointer similarRelease{ db::Release::find(LmsApp->getDbSession(), id) };
            if (!similarRelease)
                continue;

            similarReleasesContainer->addWidget(releaseListHelpers::createEntry(similarRelease));
        }
    }

} // namespace lms::ui
