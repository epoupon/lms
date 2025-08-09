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

#include <Wt/WPushButton.h>

#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/ArtistInfo.hpp"
#include "database/objects/ArtworkId.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ScanSettings.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/recommendation/IRecommendationService.hpp"

#include "ArtistListHelpers.hpp"
#include "Filters.hpp"
#include "LmsApplication.hpp"
#include "LmsApplicationException.hpp"
#include "PlayQueueController.hpp"
#include "ReleaseHelpers.hpp"
#include "TrackListHelpers.hpp"
#include "Utils.hpp"
#include "common/InfiniteScrollingContainer.hpp"
#include "resource/DownloadResource.hpp"

namespace lms::ui
{
    namespace
    {
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

        filters.updated().connect([this] {
            _needForceRefresh = true;
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
        if (!_needForceRefresh && artistId && *artistId == _artistId)
            return;

        clear();
        _artistId = {};
        _trackContainer = nullptr;
        _needForceRefresh = false;

        if (!artistId)
            throw ArtistNotFoundException{};

        const auto similarArtistIds{ core::Service<recommendation::IRecommendationService>::get()->getSimilarArtists(*artistId, { db::TrackArtistLinkType::Artist, db::TrackArtistLinkType::ReleaseArtist }, 6) };

        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Artist::pointer artist{ db::Artist::find(LmsApp->getDbSession(), *artistId) };
        if (!artist)
            throw ArtistNotFoundException{};

        LmsApp->setTitle(artist->getName());
        _artistId = *artistId;

        refreshArtwork(artist->getPreferredArtworkId());
        refreshArtistInfo();
        refreshReleases();
        refreshAppearsOnReleases();
        refreshNonReleaseTracks();
        refreshLinks(artist);
        refreshSimilarArtists(similarArtistIds);

        Wt::WContainerWidget* clusterContainers{ bindNew<Wt::WContainerWidget>("clusters") };

        {
            auto clusterTypes{ db::ClusterType::findIds(LmsApp->getDbSession()).results };
            auto clusterGroups{ artist->getClusterGroups(clusterTypes, 3) };

            for (const auto& clusters : clusterGroups)
            {
                for (const db::Cluster::pointer& cluster : clusters)
                {
                    const db::ClusterId clusterId = cluster->getId();
                    Wt::WInteractWidget* entry{ clusterContainers->addWidget(utils::createFilterCluster(clusterId)) };
                    entry->clicked().connect([this, clusterId] {
                        _filters.add(clusterId);
                    });
                }
            }
        }

        bindString("name", Wt::WString::fromUTF8(artist->getName()), Wt::TextFormat::Plain);

        bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.play"), Wt::TextFormat::XHTML)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::Play, { _artistId });
            });

        bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, { _artistId });
            });
        bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayNext, { _artistId });
            });
        bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, { _artistId });
            });
        bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
            ->setLink(Wt::WLink{ std::make_unique<DownloadArtistResource>(_artistId) });

        {
            auto isStarred{ [this] { return core::Service<feedback::IFeedbackService>::get()->isStarred(LmsApp->getUserId(), _artistId); } };

            Wt::WPushButton* starBtn{ bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star")) };
            starBtn->clicked().connect([=, this] {
                if (isStarred())
                {
                    core::Service<feedback::IFeedbackService>::get()->unstar(LmsApp->getUserId(), _artistId);
                    starBtn->setText(Wt::WString::tr("Lms.Explore.star"));
                }
                else
                {
                    core::Service<feedback::IFeedbackService>::get()->star(LmsApp->getUserId(), _artistId);
                    starBtn->setText(Wt::WString::tr("Lms.Explore.unstar"));
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
        params.setFilters(_filters.getDbFilters());
        params.setArtist(_artistId, { db::TrackArtistLinkType::ReleaseArtist }, {});
        params.setSortMethod(LmsApp->getUser()->getUIArtistReleaseSortMethod());

        const auto releases{ db::Release::findIds(LmsApp->getDbSession(), params) };
        if (!releases.results.empty())
        {
            // first pass: gather all ids and sort by release type
            for (const db::ReleaseId releaseId : releases.results)
            {
                const db::Release::pointer release{ db::Release::find(LmsApp->getDbSession(), releaseId) };

                ReleaseType releaseType{ parseReleaseType(release->getReleaseTypeNames()) };
                _releaseContainers[releaseType].releases.push_back(releaseId);
            }

            // second pass: construct widgets
            Wt::WContainerWidget* releaseContainers{ bindNew<Wt::WContainerWidget>("release-containers") };
            for (auto& [releaseType, releases] : _releaseContainers)
            {
                Wt::WTemplate* releaseContainer{ releaseContainers->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Artist.template.release-container")) };

                if (releaseType.primaryType || !releaseType.customTypes.empty())
                    releaseContainer->bindString("release-type", releaseHelpers::buildReleaseTypeString(releaseType));
                else
                    releaseContainer->bindString("release-type", Wt::WString::tr("Lms.Explore.releases")); // fallback when not tagged with MB or custom type

                releases.container = releaseContainer->bindNew<InfiniteScrollingContainer>("releases", Wt::WString::tr("Lms.Explore.Releases.template.container"));
                releases.container->onRequestElements.connect(this, [this, &releases = releases] {
                    addSomeReleases(releases);
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
        constexpr core::EnumSet<db::TrackArtistLinkType> types{
            db::TrackArtistLinkType::Artist,
            db::TrackArtistLinkType::Arranger,
            db::TrackArtistLinkType::Composer,
            db::TrackArtistLinkType::Conductor,
            db::TrackArtistLinkType::Lyricist,
            db::TrackArtistLinkType::Mixer,
            db::TrackArtistLinkType::Performer,
            db::TrackArtistLinkType::Producer,
            db::TrackArtistLinkType::Remixer,
            db::TrackArtistLinkType::Writer,
        };

        _appearsOnReleaseContainer = {};

        db::Release::FindParameters params;
        params.setFilters(_filters.getDbFilters());
        params.setArtist(_artistId, types, { db::TrackArtistLinkType::ReleaseArtist });
        params.setSortMethod(db::ReleaseSortMethod::OriginalDateDesc);

        const auto releases{ db::Release::findIds(LmsApp->getDbSession(), params) };
        if (!releases.results.empty())
        {
            Wt::WTemplate* releaseContainer{ bindNew<Wt::WTemplate>("appears-on-releases", Wt::WString::tr("Lms.Explore.Artist.template.release-container")) };
            releaseContainer->bindString("release-type", Wt::WString::tr("Lms.Explore.Artist.appears-on"));
            _appearsOnReleaseContainer.releases = releases.results;
            _appearsOnReleaseContainer.container = releaseContainer->bindNew<InfiniteScrollingContainer>("releases", Wt::WString::tr("Lms.Explore.Releases.template.container"));
            _appearsOnReleaseContainer.container->onRequestElements.connect(this, [this] {
                addSomeReleases(_appearsOnReleaseContainer);
            });
        }
        else
        {
            bindEmpty("appears-on-releases");
        }
    }

    void Artist::refreshNonReleaseTracks()
    {
        setCondition("if-has-non-release-tracks", true);
        _trackContainer = bindNew<InfiniteScrollingContainer>("tracks");
        _trackContainer->onRequestElements.connect(this, [this] {
            addSomeNonReleaseTracks();
        });

        const bool added{ addSomeNonReleaseTracks() };
        setCondition("if-has-non-release-tracks", added);
    }

    void Artist::refreshSimilarArtists(const std::vector<db::ArtistId>& similarArtistsId)
    {
        if (similarArtistsId.empty())
            return;

        setCondition("if-has-similar-artists", true);
        Wt::WContainerWidget* similarArtistsContainer{ bindNew<Wt::WContainerWidget>("similar-artists") };

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
            bindString("mbid-link", std::string{ "https://musicbrainz.org/artist/" } + std::string{ mbid->getAsString() });
        }
    }

    void Artist::addSomeReleases(ReleaseContainer& releaseContainer)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        if (const db::Artist::pointer artist{ db::Artist::find(LmsApp->getDbSession(), _artistId) })
        {
            for (std::size_t i{ static_cast<std::size_t>(releaseContainer.container->getCount()) }; i < releaseContainer.releases.size(); ++i)
            {
                const db::Release::pointer release{ db::Release::find(LmsApp->getDbSession(), releaseContainer.releases[i]) };
                releaseContainer.container->add(releaseListHelpers::createEntryForArtist(release, artist));
            }
        }

        releaseContainer.container->setHasMore(false);
    }

    bool Artist::addSomeNonReleaseTracks()
    {
        bool areTracksAdded{};

        const db::Range range{ static_cast<std::size_t>(_trackContainer->getCount()), _tracksBatchSize };

        db::Track::FindParameters params;
        params.setFilters(_filters.getDbFilters());
        params.setArtist(_artistId);
        params.setRange(range);
        params.setSortMethod(db::TrackSortMethod::Name);
        params.setNonRelease(true);

        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const auto tracks{ db::Track::find(LmsApp->getDbSession(), params) };
        for (const db::Track::pointer& track : tracks.results)
        {
            // TODO handle this with range
            if (_trackContainer->getCount() == _tracksMaxCount)
                break;

            _trackContainer->add(TrackListHelpers::createEntry(track, _playQueueController, _filters));

            areTracksAdded = true;
        }

        _trackContainer->setHasMore(tracks.moreResults);

        return areTracksAdded;
    }

} // namespace lms::ui
