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
#include <array>
#include <map>
#include <span>
#include <string>

#include <Wt/WAnchor.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WImage.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>

#include "core/String.hpp"
#include "core/Utils.hpp"

#include "database/Session.hpp"
#include "database/Types.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Movement.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ScanSettings.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/User.hpp"
#include "database/objects/Work.hpp"
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

        template<typename T, typename SameRunFunc, typename Visitor>
        void visitRuns(std::span<const T> entries, SameRunFunc sameRun, Visitor visit)
        {
            std::size_t startIndex{};
            for (std::size_t i{ 1 }; i < entries.size(); ++i)
            {
                if (!sameRun(entries[i - 1], entries[i]))
                {
                    visit(entries.subspan(startIndex, i - startIndex));
                    startIndex = i;
                }
            }
            if (!entries.empty())
                visit(entries.subspan(startIndex));
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

        constexpr std::size_t maxTagCloudItemCount{ 3 };

        Wt::WContainerWidget* clusterContainers{ bindNew<Wt::WContainerWidget>("clusters") };
        {
            db::Genre::find(session, db::Genre::FindParameters{}.setRelease(release->getId()).setSortMethod(db::GenreSortMethod::TrackCountDesc).setRange(db::Range{ 0, maxTagCloudItemCount }), [&](const db::Genre::pointer& genre) {
                const db::GenreId genreId{ genre->getId() };
                Wt::WInteractWidget* entry{ clusterContainers->addWidget(utils::createFilterGenre(genreId)) };
                entry->clicked().connect([this, genreId] {
                    _filters.set(genreId);
                });
            });

            db::Grouping::find(session, db::Grouping::FindParameters{}.setRelease(release->getId()).setSortMethod(db::GroupingSortMethod::TrackCountDesc).setRange(db::Range{ 0, maxTagCloudItemCount }), [&](const db::Grouping::pointer& grouping) {
                const db::GroupingId groupingId{ grouping->getId() };
                Wt::WInteractWidget* entry{ clusterContainers->addWidget(utils::createFilterGrouping(groupingId)) };
                entry->clicked().connect([this, groupingId] {
                    _filters.set(groupingId);
                });
            });

            db::Language::find(session, db::Language::FindParameters{}.setRelease(release->getId()).setSortMethod(db::LanguageSortMethod::TrackCountDesc).setRange(db::Range{ 0, maxTagCloudItemCount }), [&](const db::Language::pointer& language) {
                const db::LanguageId languageId{ language->getId() };
                Wt::WInteractWidget* entry{ clusterContainers->addWidget(utils::createFilterLanguage(languageId)) };
                entry->clicked().connect([this, languageId] {
                    _filters.set(languageId);
                });
            });

            db::Mood::find(session, db::Mood::FindParameters{}.setRelease(release->getId()).setSortMethod(db::MoodSortMethod::TrackCountDesc).setRange(db::Range{ 0, maxTagCloudItemCount }), [&](const db::Mood::pointer& mood) {
                const db::MoodId moodId{ mood->getId() };
                Wt::WInteractWidget* entry{ clusterContainers->addWidget(utils::createFilterMood(moodId)) };
                entry->clicked().connect([this, moodId] {
                    _filters.set(moodId);
                });
            });

            const auto clusterTypeIds{ db::ClusterType::findIds(session) };
            const auto clusterGroups{ release->getClusterGroups(clusterTypeIds, maxTagCloudItemCount) };

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

        const auto discInfoList{ createDiscInfoList(release) };
        _trackIds.clear();
        for (const DiscInfo& disc : discInfoList)
            appendTrackIds(_trackIds, disc);

        bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.play"), Wt::TextFormat::XHTML)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::Play, _trackIds);
            });

        bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, _trackIds);
            });

        bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayNext, _trackIds);
            });

        bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, _trackIds);
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

        refreshDiscs(release, discInfoList);
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

    void Release::refreshDiscs(const db::ObjectPtr<db::Release>& release, std::span<const DiscInfo> discoInfoList)
    {
        Wt::WContainerWidget* discContainer{ bindNew<Wt::WContainerWidget>("disc-container") };

        const DisplayOptions displayOptions{
            .displayTrackArtists = shouldDisplayTrackArtists(release),
            .showDiscHeaders = discoInfoList.size() > 1
                            || release->getTotalDisc().value_or(0) > 1
                            || std::any_of(std::cbegin(discoInfoList), std::cend(discoInfoList), [](const DiscInfo& discInfo) {
                                   return !discInfo.medium->getName().empty() || discInfo.medium->getPreferredArtworkId().isValid();
                               })
        };

        for (const DiscInfo& discInfo : discoInfoList)
            addDisc(discContainer, discInfo, displayOptions);
    }

    std::vector<Release::DiscInfo> Release::createDiscInfoList(const db::Release::pointer& release)
    {
        std::vector<Release::DiscInfo> discInfoList;

        const std::vector<db::Medium::pointer> mediums{ release->getMediums() };

        discInfoList.reserve(mediums.size());
        for (const db::Medium::pointer& medium : mediums)
            discInfoList.push_back(createDiscInfo(medium));

        return discInfoList;
    }

    Release::DiscInfo Release::createDiscInfo(const db::Medium::pointer& medium)
    {
        const std::vector<TrackInfo> tracks{ collectMediumTrackInfoList(medium) };

        auto trackUseWork{ [](const TrackInfo& info) { return info.work && info.movement; } };
        auto trackComputeWorkKey{ [&trackUseWork](const TrackInfo& info) { return trackUseWork(info) ? info.work->getId() : db::WorkId{}; } };

        DiscInfo disc;
        disc.medium = medium;

        visitRuns<TrackInfo>(
            tracks,
            [&](const TrackInfo& a, const TrackInfo& b) { return trackComputeWorkKey(a) == trackComputeWorkKey(b); },
            [&](std::span<const TrackInfo> entries) {
                std::vector<TrackInfo> tracks(std::cbegin(entries), std::cend(entries));

                if (trackUseWork(tracks.front()))
                {
                    std::stable_sort(std::begin(tracks), std::end(tracks), [](const TrackInfo& a, const TrackInfo& b) {
                        // fallback on track-number order
                        return a.movement->getNumber().value_or(0) < b.movement->getNumber().value_or(0);
                    });

                    disc.segments.push_back(WorkInfo{ .work = tracks.front().work, .tracks = std::move(tracks) });
                }
                else
                {
                    disc.segments.push_back(std::move(tracks));
                } });

        return disc;
    }

    void Release::appendTrackIds(std::vector<db::TrackId>& trackIds, const DiscInfo& disc)
    {
        for (const DiscInfo::Segment& segment : disc.segments)
        {
            std::visit(core::utils::overloads{
                           [&](const WorkInfo& work) {
                               appendTrackIds(trackIds, work.tracks);
                           },
                           [&](const TrackInfoList& tracks) {
                               appendTrackIds(trackIds, tracks);
                           } },
                       segment);
        }
    }

    void Release::appendTrackIds(std::vector<db::TrackId>& trackIds, const TrackInfoList& tracks)
    {
        for (const TrackInfo& track : tracks)
            trackIds.push_back(track.track->getId());
    }

    void Release::addDisc(Wt::WContainerWidget* container, const DiscInfo& discInfo, DisplayOptions displayOptions)
    {
        if (displayOptions.showDiscHeaders)
        {
            Wt::WString discTitle;
            if (discInfo.medium->getName().empty())
                discTitle = Wt::WString::tr("Lms.Explore.Release.disc").arg(discInfo.medium->getPosition().value_or(1));
            else
                discTitle = Wt::WString::fromUTF8(std::string{ discInfo.medium->getName() });

            std::vector<db::TrackId> trackIds;
            appendTrackIds(trackIds, discInfo);
            container = addSegment(container, discTitle, discInfo.medium->getPreferredArtworkId(), trackIds, "Lms.Explore.Release.template.segment-disc");
        }

        for (const DiscInfo::Segment& segment : discInfo.segments)
        {
            std::visit(core::utils::overloads{
                           [&](const WorkInfo& work) {
                               std::vector<db::TrackId> trackIds;
                               appendTrackIds(trackIds, work.tracks);

                               Wt::WContainerWidget* workContainer{ addSegment(container, Wt::WString::fromUTF8(std::string{ work.work->getName() }), db::ArtworkId{}, trackIds, "Lms.Explore.Release.template.segment-work") };
                               addTrackEntries(workContainer, work.tracks, displayOptions.displayTrackArtists);
                           },
                           [&](const TrackInfoList& tracks) {
                               std::vector<db::TrackId> trackIds;
                               appendTrackIds(trackIds, tracks);

                               // A run of tracks with no work header
                               auto* flat{ container->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Release.template.segment-flat")) };
                               addTrackEntries(flat->bindNew<Wt::WContainerWidget>("tracks"), tracks, displayOptions.displayTrackArtists);
                           } },
                       segment);
        }
    }

    Release::TrackInfoList Release::collectMediumTrackInfoList(const db::Medium::pointer& medium)
    {
        db::Track::FindParameters params;
        params.setMedium(medium->getId());
        params.setSortMethod(db::TrackSortMethod::TrackNumber);

        std::vector<TrackInfo> tracks;
        db::Track::find(LmsApp->getDbSession(), params, [&](const db::Track::pointer& track) {
            const auto works{ track->getWorks() };
            const auto movements{ track->getMovements() };

            tracks.push_back(TrackInfo{
                .track = track,
                .work = works.empty() ? db::Work::pointer{} : works.front(),
                .movement = movements.empty() ? db::Movement::pointer{} : movements.front(),
            });
        });
        return tracks;
    }

    void Release::addTrackEntries(Wt::WContainerWidget* container, std::span<const TrackInfo> tracks, bool displayTrackArtists)
    {
        for (const TrackInfo& trackInfo : tracks)
            addTrackEntry(container, trackInfo, displayTrackArtists);
    }

    void Release::addTrackEntry(Wt::WContainerWidget* container, const TrackInfo& trackInfo, bool displayTrackArtists)
    {
        const db::Track::pointer& track{ trackInfo.track };
        const db::TrackId trackId{ track->getId() };

        Template* entry{ container->addNew<Template>(Wt::WString::tr("Lms.Explore.Release.template.track")) };
        entry->addFunction("id", &Wt::WTemplate::Functions::id);

        const std::string title{ (trackInfo.movement && !trackInfo.movement->getName().empty()) ? trackInfo.movement->getName() : track->getName() };
        entry->bindString("name", Wt::WString::fromUTF8(title), Wt::TextFormat::Plain);

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

        if (trackInfo.movement && trackInfo.movement->getNumber())
        {
            const std::size_t number{ *trackInfo.movement->getNumber() };
            entry->setCondition("if-has-position", true);
            if (const std::string numeral{ core::stringUtils::toRomanNumeral(number) }; !numeral.empty())
                entry->bindString("position", Wt::WString::fromUTF8(numeral), Wt::TextFormat::Plain);
            else
                entry->bindInt("position", static_cast<int>(number));
        }
        else if (const auto n{ track->getTrackNumber() })
        {
            entry->setCondition("if-has-position", true);
            entry->bindInt("position", static_cast<int>(*n));
        }

        auto playTrack{ [this, trackId] {
            const auto it{ std::find(std::cbegin(_trackIds), std::cend(_trackIds), trackId) };
            if (it == std::cend(_trackIds))
                return;

            _playQueueController.playAtIndex(_trackIds, static_cast<std::size_t>(std::distance(std::cbegin(_trackIds), it)));
        } };

        Wt::WPushButton* playBtn{ entry->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.template.play-btn"), Wt::TextFormat::XHTML) };
        playBtn->setAttributeValue("aria-label", Wt::WString::tr("Lms.play-item").arg(title));
        playBtn->clicked().connect(playTrack);

        {
            entry->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.template.more-btn"), Wt::TextFormat::XHTML)
                ->setAttributeValue("aria-label", Wt::WString::tr("Lms.more"));
            entry->bindNew<Wt::WPushButton>("play", Wt::WString::tr("Lms.Explore.play"))
                ->clicked()
                .connect(playTrack);
            entry->bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"))
                ->clicked()
                .connect([this, trackId] {
                    db::TrackId trackIds[]{ trackId };
                    _playQueueController.processCommand(PlayQueueController::Command::PlayNext, trackIds);
                });
            entry->bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"))
                ->clicked()
                .connect([this, trackId] {
                    db::TrackId trackIds[]{ trackId };
                    _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, trackIds);
                });

            {
                auto isStarred{ [=] { return core::Service<feedback::IFeedbackService>::get()->isStarred(LmsApp->getUserId(), trackId); } };

                Wt::WPushButton* starBtn{ entry->bindNew<Wt::WPushButton>("star-btn", Wt::WString::tr(isStarred() ? "Lms.template.unstar-btn" : "Lms.template.star-btn"), Wt::TextFormat::XHTML) };
                starBtn->setAttributeValue("aria-pressed", isStarred() ? "true" : "false");
                starBtn->setAttributeValue("aria-label", Wt::WString::tr("Lms.Explore.star-item").arg(title));
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
            bindString("mbid-link", "https://musicbrainz.org/release/" + mbid->toString());
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
        if (releaseIds.size() <= 1)
            return;

        setCondition("if-has-other-versions", true);
        auto* container{ bindNew<Wt::WContainerWidget>("other-versions") };

        for (const db::ReleaseId id : releaseIds)
        {
            if (id == _releaseId)
                continue;

            const db::Release::pointer otherVersionRelease{ db::Release::find(LmsApp->getDbSession(), id) };
            if (!otherVersionRelease)
                continue;

            container->addWidget(releaseListHelpers::createEntry(otherVersionRelease, { releaseListHelpers::DisplayOptions::ShowYear }));
        }
    }

    void Release::refreshRelatedReleases(std::span<const db::ReleaseId> similarReleaseIds)
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

    Wt::WContainerWidget* Release::addSegment(Wt::WContainerWidget* container, const Wt::WString& title, db::ArtworkId artworkId, std::span<const db::TrackId> trackIds, const char* segmentTemplate)
    {
        const std::vector<db::TrackId> ids{ std::cbegin(trackIds), std::cend(trackIds) };

        auto header{ std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Release.template.segment-header")) };
        header->addFunction("id", &Wt::WTemplate::Functions::id);

        if (artworkId.isValid())
        {
            auto image{ utils::createArtworkImage(artworkId, ArtworkResource::DefaultArtworkType::Release, ArtworkResource::Size::Small) };
            header->setCondition("if-has-artwork", true);
            image->addStyleClass("Lms-cover-track rounded"); // HACK
            image->clicked().connect([artworkId] {
                utils::showArtworkModal(Wt::WLink{ LmsApp->getArtworkResource()->getArtworkUrl(artworkId, ArtworkResource::DefaultArtworkType::Release) });
            });
            header->bindWidget<Wt::WImage>("artwork", std::move(image));
        }

        header->bindNew<Wt::WText>("title", title, Wt::TextFormat::Plain);

        Wt::WPushButton* playBtn{ header->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.template.play-btn"), Wt::TextFormat::XHTML) };
        playBtn->setAttributeValue("aria-label", Wt::WString::tr("Lms.play-item").arg(title));
        playBtn->clicked().connect([this, ids] {
            _playQueueController.processCommand(PlayQueueController::Command::Play, ids);
        });
        header->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.template.more-btn"), Wt::TextFormat::XHTML)
            ->setAttributeValue("aria-label", Wt::WString::tr("Lms.more"));
        header->bindNew<Wt::WPushButton>("play", Wt::WString::tr("Lms.Explore.play"))
            ->clicked()
            .connect([this, ids] {
                _playQueueController.processCommand(PlayQueueController::Command::Play, ids);
            });
        header->bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"))
            ->clicked()
            .connect([this, ids] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayNext, ids);
            });
        header->bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this, ids] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, ids);
            });
        header->bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"))
            ->clicked()
            .connect([this, ids] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, ids);
            });

        Template* segment{ container->addNew<Template>(Wt::WString::tr(segmentTemplate)) };
        segment->bindWidget("header", std::move(header));

        return segment->bindNew<Wt::WContainerWidget>("tracks");
    }

} // namespace lms::ui
