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

#include <algorithm>
#include <map>
#include <string>

#include <Wt/WAnchor.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WImage.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>

#include "core/String.hpp"

#include "database/Session.hpp"
#include "database/Types.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ScanSettings.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/User.hpp"
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

            if (const auto artistsByRole{ utils::getTrackArtistsByRole(release) }; !artistsByRole.empty())
            {
                releaseInfo->setCondition("if-has-artist", true);
                Wt::WContainerWidget* artistTable{ releaseInfo->bindNew<Wt::WContainerWidget>("artist-table") };

                for (const auto& [role, artists] : artistsByRole)
                {
                    std::unique_ptr<Wt::WContainerWidget> artistContainer{ utils::createArtistAnchorList(artists) };
                    auto artistsEntry{ std::make_unique<Template>(Wt::WString::tr("Lms.Explore.template.info.artists")) };
                    artistsEntry->bindString("type", role);
                    artistsEntry->bindWidget("artist-container", std::move(artistContainer));
                    artistTable->addWidget(std::move(artistsEntry));
                }
            }

            // TODO make labels clickable to automatically add filters
            if (const std::vector<std::string> labels{ release->getLabelNames() }; !labels.empty())
            {
                releaseInfo->setCondition("if-has-labels", true);
                releaseInfo->bindString("release-labels", core::stringUtils::joinStrings(labels, " · "));
            }

            // Codecs
            {
                std::string codecStr;
                for (core::media::Codec codec : release->getCodecs())
                {
                    if (!codecStr.empty())
                        codecStr += " · ";
                    codecStr += core::media::getCodecDesc(codec).longName.str();
                }

                if (!codecStr.empty())
                {
                    releaseInfo->setCondition("if-has-codec", true);
                    releaseInfo->bindString("codec", codecStr, Wt::TextFormat::Plain);
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

        std::optional<db::ReleaseId> extractReleaseIdFromInternalPath()
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

            return core::stringUtils::readAs<db::ReleaseId::ValueType>(wApp->internalPathNextPart("/release/"));
        }

        void fillTrackArtistLinks(Wt::WTemplate* trackEntry, const db::Track::pointer& track)
        {
            const db::User::pointer user{ LmsApp->getUser() };
            if (!user->getUIEnableInlineArtistRelationships())
                return;

            const core::EnumSet<db::TrackArtistLinkType> inlineArtistRelationships{ user->getUIInlineArtistRelationships() };
            if (inlineArtistRelationships.empty())
                return;

            const std::map<Wt::WString, std::vector<db::Artist::pointer>> artistsByRole{ utils::getArtistsByRole(track, inlineArtistRelationships) };
            if (artistsByRole.empty())
                return;

            trackEntry->setCondition("if-has-artist-links", true);
            Wt::WContainerWidget* artistLinksContainer{ trackEntry->bindNew<Wt::WContainerWidget>("artist-links") };

            for (const auto& [role, artists] : artistsByRole)
            {
                Wt::WTemplate* artistLinkEntry{ artistLinksContainer->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.artist-links-entry")) };
                artistLinkEntry->bindString("role", role, Wt::TextFormat::Plain);
                artistLinkEntry->bindWidget("anchors", utils::createArtistAnchorList(artists));
            }
        }

        bool shouldDisplayTrackArtists(const db::Release::pointer& release)
        {
            bool res{ true };

            // TODO, just compare artist display names
            const auto trackArtists{ release->getTrackArtists(db::TrackArtistLinkType::Artist) };
            if (trackArtists.size() == 1)
            {
                const auto releaseArtists{ release->getArtists() };
                if (releaseArtists.empty() || trackArtists == releaseArtists)
                    res = false;
            }

            return res;
        }

        std::unique_ptr<Wt::WTemplate> createNoDiscContainer()
        {
            return std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.entry-nodisc"));
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

        refreshView();
    }

    void Release::refreshView()
    {
        if (!wApp->internalPathMatches("/release/"))
            return;

        const auto releaseId{ extractReleaseIdFromInternalPath() };

        // consider everything is up to date is the same release is being rendered
        if (releaseId && *releaseId == _releaseId)
            return;

        clear();
        _releaseId = {};

        if (!releaseId)
            throw ReleaseNotFoundException{};

        const auto similarReleases{ core::Service<recommendation::IRecommendationService>::get()->findSimilarReleases(*releaseId, 5) };
        std::vector<db::ReleaseId> similarReleasesIds;
        similarReleasesIds.reserve(similarReleases.size());
        std::transform(std::cbegin(similarReleases), std::cend(similarReleases), std::back_inserter(similarReleasesIds), [](const auto& result) {
            return result.id;
        });

        auto& session{ LmsApp->getDbSession() };
        auto transaction{ session.createReadTransaction() };

        const db::Release::pointer release{ db::Release::find(session, *releaseId) };
        if (!release)
            throw ReleaseNotFoundException{};

        LmsApp->setTitle(std::string{ release->getName() });
        _releaseId = *releaseId;

        refreshCopyright(release);
        refreshLinks(release);
        refreshOtherVersions(release);
        refreshRelatedReleases(similarReleasesIds);

        bindString("name", Wt::WString::fromUTF8(std::string{ release->getName() }), Wt::TextFormat::Plain);
        if (std::string_view comment{ release->getComment() }; !comment.empty())
        {
            setCondition("if-has-release-comment", true);
            bindString("comment", Wt::WString::fromUTF8(std::string{ comment }), Wt::TextFormat::Plain);
        }

        Wt::WString year{ releaseHelpers::buildReleaseYearString(release->getYear(), release->getOriginalYear()) };
        if (!year.empty())
        {
            setCondition("if-has-year", true);
            bindString("year", year, Wt::TextFormat::Plain);
        }

        bindString("duration", utils::durationToString(release->getDuration()), Wt::TextFormat::Plain);

        refreshReleaseArtists(release);
        refreshArtwork(release->getPreferredArtworkId());

        Wt::WContainerWidget* clusterContainers{ bindNew<Wt::WContainerWidget>("clusters") };
        {
            const auto clusterTypeIds{ db::ClusterType::findIds(session).results };
            const auto clusterGroups{ release->getClusterGroups(clusterTypeIds, 3) };

            for (const auto& clusters : clusterGroups)
            {
                for (const db::Cluster::pointer& cluster : clusters)
                {
                    const db::ClusterId clusterId{ cluster->getId() };
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

        if (LmsApp->areDownloadsEnabled())
        {
            setCondition("if-has-download", true);
            bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
                ->setLink(Wt::WLink{ std::make_unique<DownloadReleaseResource>(_releaseId) });
        }

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

        refreshDiscs(release);
    }

    void Release::refreshArtwork(db::ArtworkId artworkId)
    {
        std::unique_ptr<Wt::WImage> artworkImage;
        if (artworkId.isValid())
        {
            artworkImage = utils::createArtworkImage(artworkId, ArtworkResource::DefaultArtworkType::Release, ArtworkResource::Size::Large);
            artworkImage->addStyleClass("Lms-cursor-pointer"); // HACK
        }
        else
            artworkImage = utils::createDefaultArtworkImage(ArtworkResource::DefaultArtworkType::Release);

        auto* image{ bindWidget<Wt::WImage>("artwork", std::move(artworkImage)) };
        if (artworkId.isValid())
        {
            image->clicked().connect([artworkId] {
                utils::showArtworkModal(Wt::WLink{ LmsApp->getArtworkResource()->getArtworkUrl(artworkId, ArtworkResource::DefaultArtworkType::Release) });
            });
        }
    }

    void Release::refreshReleaseArtists(const db::Release::pointer& release)
    {
        if (auto container{ utils::createArtistsAnchors(release) })
        {
            setCondition("if-has-release-artists", true);
            bindWidget("artists", std::move(container));
        }
    }

    void Release::refreshDiscs(const db::Release::pointer& release)
    {
        Wt::WContainerWidget* discsContainer{ bindNew<Wt::WContainerWidget>("disc-container") };
        const std::vector<db::Medium::pointer> mediums{ release->getMediums() };
        if (mediums.empty())
            return;

        const bool displayTrackArtists{ shouldDisplayTrackArtists(release) };
        const auto totalDisc{ release->getTotalDisc() };

        const bool createDiscs{ mediums.size() > 1
                                || !mediums[0]->getName().empty()
                                || totalDisc > 1 };

        for (const db::Medium::pointer& medium : mediums)
        {
            Wt::WTemplate* disc{ discsContainer->addWidget<Wt::WTemplate>(createDiscs ? createDisc(medium) : createNoDiscContainer()) };

            Wt::WContainerWidget* tracksContainer{ disc->bindNew<Wt::WContainerWidget>("tracks") };
            createTracks(tracksContainer, medium, displayTrackArtists);
        }
    }

    void Release::createTracks(Wt::WContainerWidget* tracksContainer, const db::Medium::pointer& medium, bool displayTrackArtists)
    {
        db::Track::FindParameters params;
        params.setMedium(medium->getId());
        params.setSortMethod(db::TrackSortMethod::TrackNumber);

        db::Track::find(LmsApp->getDbSession(), params, [&](const db::Track::pointer& track) {
            const db::TrackId trackId{ track->getId() };

            Template* entry{ tracksContainer->addNew<Template>(Wt::WString::tr("Lms.Explore.Release.template.entry")) };
            entry->addFunction("id", &Wt::WTemplate::Functions::id);

            entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

            if (displayTrackArtists)
            {
                const auto artistDisplayInfo{ utils::computeArtistDisplayInfo(track, db::TrackArtistLinkType::Artist) };
                if (!artistDisplayInfo.entries.empty())
                {
                    entry->setCondition("if-has-artists", true);
                    entry->bindWidget("artists", utils::createArtistsAnchors(artistDisplayInfo));
                    entry->bindWidget("artists-md", utils::createArtistsAnchors(artistDisplayInfo));
                }
            }

            fillTrackArtistLinks(entry, track);

            auto trackNumber{ track->getTrackNumber() };
            if (trackNumber)
            {
                entry->setCondition("if-has-track-number", true);
                entry->bindInt("track-number", *trackNumber);
            }

            Wt::WPushButton* playBtn{ entry->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.template.play-btn"), Wt::TextFormat::XHTML) };
            playBtn->setAttributeValue("aria-label", Wt::WString::tr("Lms.play-item").arg(track->getName()));
            playBtn->clicked().connect([this, trackId] {
                _playQueueController.playTrackInRelease(trackId);
            });

            {
                entry->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.template.more-btn"), Wt::TextFormat::XHTML)
                    ->setAttributeValue("aria-label", Wt::WString::tr("Lms.more"));
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

                {
                    auto isStarred{ [=] { return core::Service<feedback::IFeedbackService>::get()->isStarred(LmsApp->getUserId(), trackId); } };

                    Wt::WPushButton* starBtn{ entry->bindNew<Wt::WPushButton>("star-btn", Wt::WString::tr(isStarred() ? "Lms.template.unstar-btn" : "Lms.template.star-btn"), Wt::TextFormat::XHTML) };
                    starBtn->setAttributeValue("aria-pressed", isStarred() ? "true" : "false");
                    starBtn->setAttributeValue("aria-label", Wt::WString::tr("Lms.Explore.star-item").arg(track->getName()));
                    Wt::WPushButton* starMenuEntry{ entry->bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star")) };

                    auto toggle{ [=] {
                        auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                        if (isStarred())
                        {
                            core::Service<feedback::IFeedbackService>::get()->unstar(LmsApp->getUserId(), trackId);
                            starMenuEntry->setText(Wt::WString::tr("Lms.Explore.star"));
                            starBtn->setText(Wt::WString::tr("Lms.template.star-btn"));
                            starBtn->setAttributeValue("aria-pressed", "false");
                        }
                        else
                        {
                            core::Service<feedback::IFeedbackService>::get()->star(LmsApp->getUserId(), trackId);
                            starMenuEntry->setText(Wt::WString::tr("Lms.Explore.unstar"));
                            starBtn->setText(Wt::WString::tr("Lms.template.unstar-btn"));
                            starBtn->setAttributeValue("aria-pressed", "true");
                        }
                    } };

                    starMenuEntry->clicked().connect([=] { toggle(); });
                    starBtn->clicked().connect([=] { toggle(); });
                }

                if (LmsApp->areDownloadsEnabled())
                {
                    entry->setCondition("if-has-download", true);
                    entry->bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
                        ->setLink(Wt::WLink{ std::make_unique<DownloadTrackResource>(trackId) });
                }

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

            LmsApp->getMediaPlayer().trackLoaded.connect(entry, [=](db::TrackId loadedTrackId) {
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

    void Release::refreshCopyright(const db::Release::pointer& release)
    {
        std::optional<std::string> copyright;
        std::optional<std::string> copyrightURL;

        if (release->hasVariousCopyrights())
        {
            copyright = Wt::WString::tr("Lms.Explore.various-copyrights").toUTF8();
        }
        else
        {
            copyright = release->getCopyright();
            copyrightURL = release->getCopyrightURL();
        }

        if (auto copyrightWidget{ utils::createCopyright(copyright ? *copyright : "", copyrightURL ? *copyrightURL : "") })
        {
            setCondition("if-has-copyright", true);
            bindWidget("copyright", std::move(copyrightWidget));
        }
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

    void Release::refreshOtherVersions(const db::Release::pointer& release)
    {
        const auto groupMBID{ release->getGroupMBID() };
        if (!groupMBID)
            return;

        db::Release::FindParameters params;
        params.setReleaseGroupMBID(groupMBID);
        params.setSortMethod(db::ReleaseSortMethod::DateAsc);

        const auto releaseIds{ db::Release::findIds(LmsApp->getDbSession(), params) };
        if (releaseIds.results.size() <= 1)
            return;

        setCondition("if-has-other-versions", true);
        auto* container{ bindNew<Wt::WContainerWidget>("other-versions") };

        for (const db::ReleaseId id : releaseIds.results)
        {
            if (id == _releaseId)
                continue;

            const db::Release::pointer otherVersionRelease{ db::Release::find(LmsApp->getDbSession(), id) };
            if (!otherVersionRelease)
                continue;

            container->addWidget(releaseListHelpers::createEntry(otherVersionRelease, { releaseListHelpers::DisplayOptions::ShowYear }));
        }
    }

    void Release::refreshRelatedReleases(const std::vector<db::ReleaseId>& similarReleaseIds)
    {
        if (similarReleaseIds.empty())
            return;

        setCondition("if-has-related-releases", true);
        auto* similarReleasesContainer{ bindNew<Wt::WContainerWidget>("related-releases") };

        for (const db::ReleaseId id : similarReleaseIds)
        {
            const db::Release::pointer similarRelease{ db::Release::find(LmsApp->getDbSession(), id) };
            if (!similarRelease)
                continue;

            similarReleasesContainer->addWidget(releaseListHelpers::createEntry(similarRelease, { releaseListHelpers::DisplayOptions::ShowArtist }));
        }
    }

    std::unique_ptr<Wt::WTemplate> Release::createDisc(const db::Medium::pointer& medium)
    {
        const db::MediumId mediumId{ medium->getId() };

        std::unique_ptr<Wt::WTemplate> disc{ std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Release.template.entry-disc")) };
        disc->addFunction("id", &Wt::WTemplate::Functions::id);

        if (db::ArtworkId artworkId{ medium->getPreferredArtworkId() }; artworkId.isValid())
        {
            auto image{ utils::createArtworkImage(artworkId, ArtworkResource::DefaultArtworkType::Release, ArtworkResource::Size::Small) };

            disc->setCondition("if-has-artwork", true);

            image->addStyleClass("Lms-cover-track rounded"); // HACK
            image->clicked().connect([artworkId] {
                utils::showArtworkModal(Wt::WLink{ LmsApp->getArtworkResource()->getArtworkUrl(artworkId, ArtworkResource::DefaultArtworkType::Release) });
            });
            disc->bindWidget<Wt::WImage>("artwork", std::move(image));
        }

        Wt::WString discTitle;
        if (medium->getName().empty())
            discTitle = Wt::WString::tr("Lms.Explore.Release.disc").arg(medium->getPosition() ? *medium->getPosition() : 1 /* TODO */);
        else
            discTitle = Wt::WString::fromUTF8(std::string{ medium->getName() });
        disc->bindNew<Wt::WText>("disc-title", discTitle, Wt::TextFormat::Plain);

        Wt::WPushButton* playBtn{ disc->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.template.play-btn"), Wt::TextFormat::XHTML) };
        playBtn->setAttributeValue("aria-label", Wt::WString::tr("Lms.play-item").arg(discTitle));
        playBtn->clicked().connect([this, mediumId] {
            _playQueueController.processCommand(PlayQueueController::Command::Play, mediumId);
        });
        disc->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.template.more-btn"), Wt::TextFormat::XHTML)
            ->setAttributeValue("aria-label", Wt::WString::tr("Lms.more"));
        ;
        disc->bindNew<Wt::WPushButton>("play", Wt::WString::tr("Lms.Explore.play"))
            ->clicked()
            .connect([this, mediumId] {
                _playQueueController.processCommand(PlayQueueController::Command::Play, mediumId);
            });
        disc->bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"))
            ->clicked()
            .connect([this, mediumId] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayNext, mediumId);
            });

        disc->bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this, mediumId] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, mediumId);
            });
        disc->bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"))
            ->clicked()
            .connect([this, mediumId] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, mediumId);
            });

        return disc;
    }
} // namespace lms::ui
