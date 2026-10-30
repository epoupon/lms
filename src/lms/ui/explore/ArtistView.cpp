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

#include "ArtistView.hpp"

#include <algorithm>
#include <array>

#include <Wt/WPushButton.h>
#include <Wt/WStackedWidget.h>

#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/ArtistInfo.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ScanSettings.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/recommendation/IRecommendationService.hpp"

#include "ArtistListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "LmsApplicationException.hpp"
#include "PlayQueueController.hpp"
#include "ReleaseHelpers.hpp"
#include "TrackArtistTypeSelector.hpp"
#include "TrackListHelpers.hpp"
#include "Utils.hpp"
#include "common/InfiniteScrollingContainer.hpp"
#include "resource/DownloadResource.hpp"

namespace lms::ui
{
    namespace
    {
        constexpr std::size_t maxTagCloudItems{ 3 };

        std::optional<db::ArtistId> extractArtistIdFromInternalPath()
        {
            if (wApp->internalPathMatches("/artist/mbid/"))
            {
                const auto mbid{ core::UUID::fromString(wApp->internalPathNextPart("/artist/mbid/")) };
                if (mbid)
                {
                    auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                    if (const db::Artist::pointer artist{ db::Artist::find(LmsApp->getDbSession(), *mbid) })
                        return artist->getId();
                }

                return std::nullopt;
            }

            return core::stringUtils::readAs<db::ArtistId::ValueType>(wApp->internalPathNextPart("/artist/"));
        }
    } // namespace

    Artist::Artist(Filters& filters, PlayQueueController& controller)
        : Template{ Wt::WString::tr("Lms.Explore.Artist.template") }
        , _filters{ filters }
        , _playQueueController{ controller }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);
        addFunction("id", &Wt::WTemplate::Functions::id);

        LmsApp->internalPathChanged().connect(this, [this] {
            refreshView();
        });

        refreshView();
    }

    void Artist::refreshView()
    {
        if (!wApp->internalPathMatches("/artist/"))
            return;

        const auto artistId{ extractArtistIdFromInternalPath() };

        // consider everything is up to date is the same artist is being rendered
        if (artistId && *artistId == _artistId)
            return;

        clear();
        _artistId = {};
        _trackContainer = nullptr;
        _artistName = {};

        if (!artistId)
            throw ArtistNotFoundException{};

        const auto similarArtists{ core::Service<recommendation::IRecommendationService>::get()->findSimilarArtists(*artistId, { db::TrackArtistLinkType::Artist }, 6) };
        std::vector<db::ArtistId> similarArtistIds;
        similarArtistIds.reserve(similarArtists.size());
        std::transform(std::cbegin(similarArtists), std::cend(similarArtists), std::back_inserter(similarArtistIds), [](const auto& result) {
            return result.id;
        });

        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Artist::pointer artist{ db::Artist::find(LmsApp->getDbSession(), *artistId) };
        if (!artist)
            throw ArtistNotFoundException{};

        LmsApp->setTitle(artist->getName());
        _artistId = *artistId;
        _artistName = artist->getName();

        refreshArtwork(artist->getPreferredArtworkId());
        refreshArtistInfo();
        refreshReleases();
        refreshAppearsOnReleases();
        refreshNonReleaseTracks();
        refreshLinks(artist);
        refreshRelatedArtists(similarArtistIds);

        Wt::WContainerWidget* clusterContainers{ bindNew<Wt::WContainerWidget>("clusters") };

        {
            db::Genre::find(LmsApp->getDbSession(), db::Genre::FindParameters{}.setArtist(_artistId).setSortMethod(db::GenreSortMethod::TrackCountDesc).setRange(db::Range{ 0, maxTagCloudItems }), [&](const db::Genre::pointer& genre) {
                const db::GenreId genreId{ genre->getId() };
                Wt::WInteractWidget* entry{ clusterContainers->addWidget(utils::createFilterGenre(genreId)) };
                entry->clicked().connect([this, genreId] {
                    _filters.set(genreId);
                });
            });

            db::Grouping::find(LmsApp->getDbSession(), db::Grouping::FindParameters{}.setArtist(_artistId).setSortMethod(db::GroupingSortMethod::TrackCountDesc).setRange(db::Range{ 0, maxTagCloudItems }), [&](const db::Grouping::pointer& grouping) {
                const db::GroupingId groupingId{ grouping->getId() };
                Wt::WInteractWidget* entry{ clusterContainers->addWidget(utils::createFilterGrouping(groupingId)) };
                entry->clicked().connect([this, groupingId] {
                    _filters.set(groupingId);
                });
            });

            db::Language::find(LmsApp->getDbSession(), db::Language::FindParameters{}.setArtist(_artistId).setSortMethod(db::LanguageSortMethod::TrackCountDesc).setRange(db::Range{ 0, maxTagCloudItems }), [&](const db::Language::pointer& language) {
                const db::LanguageId languageId{ language->getId() };
                Wt::WInteractWidget* entry{ clusterContainers->addWidget(utils::createFilterLanguage(languageId)) };
                entry->clicked().connect([this, languageId] {
                    _filters.set(languageId);
                });
            });

            db::Mood::find(LmsApp->getDbSession(), db::Mood::FindParameters{}.setArtist(_artistId).setSortMethod(db::MoodSortMethod::TrackCountDesc).setRange(db::Range{ 0, maxTagCloudItems }), [&](const db::Mood::pointer& mood) {
                const db::MoodId moodId{ mood->getId() };
                Wt::WInteractWidget* entry{ clusterContainers->addWidget(utils::createFilterMood(moodId)) };
                entry->clicked().connect([this, moodId] {
                    _filters.set(moodId);
                });
            });

            for (const db::ClusterTypeId clusterTypeId : db::ClusterType::findIds(LmsApp->getDbSession()))
            {
                db::Cluster::find(LmsApp->getDbSession(), db::Cluster::FindParameters{}.setArtist(_artistId).setClusterType(clusterTypeId).setSortMethod(db::ClusterSortMethod::TrackCountDesc).setRange(db::Range{ 0, maxTagCloudItems }), [&](const db::Cluster::pointer& cluster) {
                    const db::ClusterId clusterId{ cluster->getId() };
                    Wt::WInteractWidget* entry{ clusterContainers->addWidget(utils::createFilterCluster(clusterId)) };
                    entry->clicked().connect([this, clusterId] {
                        _filters.add(clusterId);
                    });
                });
            }
        }

        bindString("name", Wt::WString::fromUTF8(_artistName), Wt::TextFormat::Plain);

        bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.play"), Wt::TextFormat::XHTML)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::Play, getTracksToPlay());
            });

        bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, getTracksToPlay());
            });
        bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayNext, getTracksToPlay());
            });
        bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, getTracksToPlay());
            });

        if (LmsApp->areDownloadsEnabled())
        {
            setCondition("if-has-download", true);
            bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
                ->setLink(Wt::WLink{ std::make_unique<DownloadArtistResource>(_artistId) });
        }

        {
            auto hasFeedback{ [this] { return core::Service<feedback::IFeedbackService>::get()->getFeedback(LmsApp->getUserId(), _artistId) == db::FeedbackValue::Loved; } };

            Wt::WPushButton* feedbackBtn{ bindNew<Wt::WPushButton>("star", Wt::WString::tr(hasFeedback() ? "Lms.Explore.unstar" : "Lms.Explore.star")) };
            feedbackBtn->clicked().connect([=, this] {
                if (hasFeedback())
                {
                    core::Service<feedback::IFeedbackService>::get()->setFeedback(LmsApp->getUserId(), _artistId, db::FeedbackValue::None);
                    feedbackBtn->setText(Wt::WString::tr("Lms.Explore.star"));
                }
                else
                {
                    core::Service<feedback::IFeedbackService>::get()->setFeedback(LmsApp->getUserId(), _artistId, db::FeedbackValue::Loved);
                    feedbackBtn->setText(Wt::WString::tr("Lms.Explore.unstar"));
                }
            });
        }
    }

    void Artist::refreshArtwork(db::ArtworkId artworkId)
    {
        std::unique_ptr<Wt::WImage> artworkImage;
        if (artworkId.isValid())
        {
            artworkImage = utils::createArtworkImage(artworkId, ArtworkResource::DefaultArtworkType::Artist, ArtworkResource::Size::Large);
            artworkImage->addStyleClass("Lms-cursor-pointer"); // HACK
        }
        else
            artworkImage = utils::createDefaultArtworkImage(ArtworkResource::DefaultArtworkType::Artist);

        auto* image{ bindWidget<Wt::WImage>("artwork", std::move(artworkImage)) };
        if (artworkId.isValid())
        {
            image->clicked().connect([artworkId] {
                utils::showArtworkModal(Wt::WLink{ LmsApp->getArtworkResource()->getArtworkUrl(artworkId, ArtworkResource::DefaultArtworkType::Artist) });
            });
        }
    }

    void Artist::refreshArtistInfo()
    {
        db::ArtistInfo::find(LmsApp->getDbSession(), _artistId, db::Range{ .offset = 0, .size = 1 }, [this](const db::ArtistInfo::pointer& _info) {
            if (!_info->getBiography().empty())
            {
                setCondition("if-has-biography", true);
                Wt::WText* bio{ bindNew<Wt::WText>("biography", std::string{ _info->getBiography() }, Wt::TextFormat::Plain) };
                bio->setInline(false);
                bio->setToolTip(tr("Lms.Explore.Artist.biography"));
                bio->clicked().connect([bio] {
                    bio->toggleStyleClass("Lms-multiline-clamp", !bio->hasStyleClass("Lms-multiline-clamp")); // hack
                });
            }
        });
    }

    void Artist::refreshReleases()
    {
        _releaseContainers.clear();

        db::Release::FindParameters params;
        params.setArtist(_artistId);
        params.setSortMethod(LmsApp->getUser()->getUIArtistReleaseSortMethod());

        // first pass: gather all ids and sort by release type
        bool hasReleaseWithDifferentName{};
        db::Release::find(LmsApp->getDbSession(), params, [&](const db::Release::pointer& release) {
            ReleaseType releaseType{ parseReleaseType(release->getReleaseTypeNames()) };
            _releaseContainers[releaseType].releases.push_back(release->getId());

            if (release->getArtistDisplayName() != _artistName)
                hasReleaseWithDifferentName = true;
        });

        if (!_releaseContainers.empty())
        {
            core::EnumSet displayOptions{ releaseListHelpers::DisplayOptions::ShowYearAndOriginalYear };
            if (hasReleaseWithDifferentName)
                displayOptions.insert(releaseListHelpers::DisplayOptions::ShowArtist);

            // second pass: construct widgets
            Wt::WContainerWidget* releaseContainers{ bindNew<Wt::WContainerWidget>("release-containers") };
            for (auto& [releaseType, releases] : _releaseContainers)
            {
                Wt::WTemplate* releaseContainer{ releaseContainers->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Artist.template.release-container")) };

                releaseContainer->bindString("release-type", releaseHelpers::buildReleaseTypeString(releaseType));
                releases.container = releaseContainer->bindNew<InfiniteScrollingContainer>("releases", Wt::WString::tr("Lms.Explore.Releases.template.container"));
                releases.container->onRequestElements.connect(this, [&, displayOptions] {
                    addSomeReleases(releases, displayOptions);
                });
            }
        }
        else
        {
            bindEmpty("release-containers");
        }
    }

    void Artist::refreshAppearsOnReleases()
    {
        _appearsOnReleases.clear();
        _appearsOnReleaseContainer = {};

        auto addToReleaseContainer = [&](std::optional<db::TrackArtistLinkType> linkType) -> bool {
            db::Release::FindParameters params;

            core::EnumSet<db::TrackArtistLinkType> linkTypes;
            if (linkType)
                linkTypes.insert(*linkType);

            params.setTrackArtist(_artistId, linkTypes);
            params.setSortMethod(db::ReleaseSortMethod::OriginalDateDesc);

            bool found{};
            db::Release::find(LmsApp->getDbSession(), params, [&](const db::Release::pointer& release) {
                if (release->hasArtist(_artistId))
                    return;

                _appearsOnReleases[linkType].push_back(release->getId());
                found = true;
            });

            return found;
        };

        constexpr std::array<std::optional<db::TrackArtistLinkType>, 9> types{
            std::nullopt,
            db::TrackArtistLinkType::Artist,
            db::TrackArtistLinkType::Composer,
            db::TrackArtistLinkType::Conductor,
            db::TrackArtistLinkType::Lyricist,
            db::TrackArtistLinkType::Mixer,
            db::TrackArtistLinkType::Performer,
            db::TrackArtistLinkType::Producer,
            db::TrackArtistLinkType::Remixer,
        };

        core::EnumSet<db::TrackArtistLinkType> foundTypes;
        for (const auto type : types)
        {
            if (addToReleaseContainer(type) && type)
                foundTypes.insert(*type);
        }

        if (!_appearsOnReleases.empty())
        {
            std::optional<db::TrackArtistLinkType> defaultType{ std::nullopt };
            if (_appearsOnReleases.size() == 2)
            {
                auto it{ std::find_if(std::cbegin(_appearsOnReleases), std::cend(_appearsOnReleases), [=](const auto& entry) { return entry.first != defaultType; }) };
                if (it != std::cend(_appearsOnReleases))
                    defaultType = it->first;
            }

            Wt::WTemplate* appearsOnContainer{ bindNew<Wt::WTemplate>("appears-on-releases", Wt::WString::tr("Lms.Explore.Artist.template.appears-on-container")) };
            appearsOnContainer->addFunction("tr", &Wt::WTemplate::Functions::tr);

            _appearsOnReleaseContainer.container = appearsOnContainer->bindNew<InfiniteScrollingContainer>("releases", Wt::WString::tr("Lms.Explore.Releases.template.container"));
            _appearsOnReleaseContainer.releases = _appearsOnReleases[defaultType];
            _appearsOnReleaseContainer.container->onRequestElements.connect(this, [&] {
                addSomeReleases(_appearsOnReleaseContainer, { releaseListHelpers::DisplayOptions::ShowYearAndOriginalYear, releaseListHelpers::DisplayOptions::ShowArtist });
            });

            TrackArtistTypeSelector* trackArtistTypeSelector{ appearsOnContainer->bindNew<TrackArtistTypeSelector>("artist-type", defaultType, foundTypes) };
            trackArtistTypeSelector->itemSelected.connect([this](std::optional<db::TrackArtistLinkType> newArtistLinkType) {
                _appearsOnReleaseContainer.releases = _appearsOnReleases[newArtistLinkType];
                _appearsOnReleaseContainer.container->reset();
            });
        }
        else
            bindEmpty("appears-on-releases");
    }

    void Artist::refreshNonReleaseTracks()
    {
        db::Track::FindParameters params;
        params.setArtist(_artistId);
        params.setSortMethod(db::TrackSortMethod::Name);
        params.setNonRelease(true);

        _nonReleaseTracks = db::Track::findIds(LmsApp->getDbSession(), params);
        if (_nonReleaseTracks.empty())
            return;

        setCondition("if-has-non-release-tracks", true);
        _trackContainer = bindNew<InfiniteScrollingContainer>("tracks");
        _trackContainer->onRequestElements.connect(this, [this] {
            addSomeNonReleaseTracks();
        });
    }

    void Artist::refreshRelatedArtists(const std::vector<db::ArtistId>& similarArtistsId)
    {
        if (similarArtistsId.empty())
            return;

        setCondition("if-has-related-artists", true);
        Wt::WContainerWidget* similarArtistsContainer{ bindNew<Wt::WContainerWidget>("related-artists") };

        for (const db::ArtistId artistId : similarArtistsId)
        {
            const db::Artist::pointer similarArtist{ db::Artist::find(LmsApp->getDbSession(), artistId) };
            if (!similarArtist)
                continue;

            similarArtistsContainer->addWidget(ArtistListHelpers::createEntry(similarArtist));
        }
    }

    void Artist::refreshLinks(const db::Artist::pointer& artist)
    {
        const auto mbid{ artist->getMBID() };
        if (mbid)
        {
            setCondition("if-has-mbid", true);
            bindString("mbid-link", "https://musicbrainz.org/artist/" + mbid->toString());
        }
    }

    void Artist::addSomeReleases(ReleaseContainer& releaseContainer, core::EnumSet<releaseListHelpers::DisplayOptions> displayOptions)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        if (const db::Artist::pointer artist{ db::Artist::find(LmsApp->getDbSession(), _artistId) })
        {
            for (std::size_t i{ static_cast<std::size_t>(releaseContainer.container->getCount()) }; i < releaseContainer.releases.size(); ++i)
            {
                const db::Release::pointer release{ db::Release::find(LmsApp->getDbSession(), releaseContainer.releases[i]) };
                releaseContainer.container->add(releaseListHelpers::createEntry(release, displayOptions));
            }
        }

        releaseContainer.container->setHasMore(false);
    }

    void Artist::addSomeNonReleaseTracks()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const std::size_t startIndex{ static_cast<std::size_t>(_trackContainer->getCount()) };
        const std::size_t endIndex{ std::min(startIndex + _tracksBatchSize, _nonReleaseTracks.size()) };

        for (std::size_t index{ startIndex }; index < endIndex; ++index)
        {
            if (const db::Track::pointer track{ db::Track::find(LmsApp->getDbSession(), _nonReleaseTracks[index]) })
                _trackContainer->add(TrackListHelpers::createEntry(track, _playQueueController, _filters));
        }

        _trackContainer->setHasMore(endIndex < _nonReleaseTracks.size());
    }

    // Own releases whole, then only the artist's tracks on the appears-on releases
    std::vector<db::TrackId> Artist::getTracksToPlay() const
    {
        std::vector<db::TrackId> tracks;

        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        auto addReleaseTracks{ [&](db::ReleaseId releaseId, db::ArtistId artistId) {
            db::Track::FindParameters params;
            params.setRelease(releaseId);
            params.setArtist(artistId);
            params.setSortMethod(db::TrackSortMethod::Release);

            const std::vector<db::TrackId> releaseTracks{ db::Track::findIds(LmsApp->getDbSession(), params) };
            tracks.insert(std::cend(tracks), std::cbegin(releaseTracks), std::cend(releaseTracks));
        } };

        for (const auto& [releaseType, releaseContainer] : _releaseContainers)
        {
            for (const db::ReleaseId releaseId : releaseContainer.releases)
                addReleaseTracks(releaseId, db::ArtistId{});
        }

        // all roles, whatever the role selected in the appears-on container
        if (const auto it{ _appearsOnReleases.find(std::nullopt) }; it != std::cend(_appearsOnReleases))
        {
            for (const db::ReleaseId releaseId : it->second)
                addReleaseTracks(releaseId, _artistId);
        }

        tracks.insert(std::cend(tracks), std::cbegin(_nonReleaseTracks), std::cend(_nonReleaseTracks));

        return tracks;
    }
} // namespace lms::ui
