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

#include "Utils.hpp"

#include <iomanip>
#include <sstream>

#include <Wt/WAnchor.h>
#include <Wt/WText.h>

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Release.hpp"
#include "database/ScanSettings.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/TrackList.hpp"
#include "explore/Filters.hpp"
#include "LmsApplication.hpp"

namespace lms::ui::utils
{
    std::string durationToString(std::chrono::milliseconds msDuration)
    {
        const std::chrono::seconds duration{ std::chrono::duration_cast<std::chrono::seconds>(msDuration) };

        std::ostringstream oss;

        if (duration.count() >= 3600)
        {
            oss << (duration.count() / 3600)
                << ":"
                << std::setfill('0') << std::setw(2) << (duration.count() % 3600) / 60
                << ":"
                << std::setfill('0') << std::setw(2) << duration.count() % 60;
        }
        else
        {
            oss << (duration.count() / 60)
                << ":"
                << std::setfill('0') << std::setw(2) << duration.count() % 60;
        }

        return oss.str();
    }


    std::unique_ptr<Wt::WImage> createCover(db::ReleaseId releaseId, CoverResource::Size size)
    {
        auto cover{ std::make_unique<Wt::WImage>() };
        cover->setImageLink(LmsApp->getCoverResource()->getReleaseUrl(releaseId, size));
        cover->setStyleClass("Lms-cover img-fluid"); // HACK
        cover->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)"); // HACK
        return cover;
    }

    std::unique_ptr<Wt::WImage> createCover(db::TrackId trackId, CoverResource::Size size)
    {
        auto cover{ std::make_unique<Wt::WImage>() };
        cover->setImageLink(LmsApp->getCoverResource()->getTrackUrl(trackId, size));
        cover->setStyleClass("Lms-cover img-fluid"); // HACK
        cover->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)"); // HACK
        return cover;
    }

    std::unique_ptr<Wt::WInteractWidget> createCluster(db::ClusterId clusterId, bool canDelete)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Cluster::pointer cluster{ db::Cluster::find(LmsApp->getDbSession(), clusterId) };
        if (!cluster)
            return {};

        auto getStyleClass{ [](const db::Cluster::pointer& cluster) -> const char*
        {
            switch (cluster->getType()->getId().getValue() % 8)
            {
                case 0: return "bg-primary";
                case 1: return "bg-secondary";
                case 2: return "bg-success";
                case 3: return "bg-danger";
                case 4: return "bg-warning text-dark";
                case 5: return "bg-info text-dark";
                case 6: return "bg-light text-dark";
                case 7: return "bg-dark";
            }

            return "bg-primary";
        } };

        const std::string styleClass{ getStyleClass(cluster) };
        auto res{ std::make_unique<Wt::WText>(std::string {} + (canDelete ? "<i class=\"fa fa-times-circle\"></i> " : "") + Wt::WString::fromUTF8(std::string{ cluster->getName() }), Wt::TextFormat::UnsafeXHTML) };

        res->setStyleClass("Lms-badge-cluster badge me-1 " + styleClass); // HACK
        res->setToolTip(std::string{ cluster->getType()->getName() }, Wt::TextFormat::Plain);
        res->setInline(true);

        return res;
    }

    std::unique_ptr<Wt::WContainerWidget> createArtistAnchorList(const std::vector<db::ArtistId>& artistIds, std::string_view cssAnchorClass)
    {
        using namespace db;

        std::unique_ptr<Wt::WContainerWidget> artistContainer{ std::make_unique<Wt::WContainerWidget>() };

        bool firstArtist{ true };

        auto transaction{ LmsApp->getDbSession().createReadTransaction() };
        for (const ArtistId artistId : artistIds)
        {
            const Artist::pointer artist{ Artist::find(LmsApp->getDbSession(), artistId) };
            if (!artist)
                continue;

            if (!firstArtist)
                artistContainer->addNew<Wt::WText>(" · ");

            auto anchor{ createArtistAnchor(artist) };
            anchor->addStyleClass("text-decoration-none"); // hack
            anchor->addStyleClass(std::string{ cssAnchorClass });
            artistContainer->addWidget(std::move(anchor));
            firstArtist = false;
        }

        return artistContainer;
    }

    std::unique_ptr<Wt::WContainerWidget> createArtistDisplayNameWithAnchors(std::string_view displayName, const std::vector<db::ArtistId>& artistIds, std::string_view cssAnchorClass)
    {
        using namespace db;

        std::size_t matchCount{};
        std::string_view::size_type currentOffset{};

        auto result{ std::make_unique<Wt::WContainerWidget>() };
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        // consider order is guaranteed + we will likely succeed
        for (const ArtistId artistId : artistIds)
        {
            const Artist::pointer artist{ Artist::find(LmsApp->getDbSession(), artistId) };
            if (!artist)
                break;

            const auto pos{ displayName.find(artist->getName(), currentOffset) };
            if (pos == std::string_view::npos)
                break;

            assert(pos >= currentOffset);
            if (pos != currentOffset)
                result->addNew<Wt::WText>(std::string{ displayName.substr(currentOffset, pos - currentOffset) }, Wt::TextFormat::Plain);

            auto anchor{ createArtistAnchor(artist) };
            anchor->addStyleClass("text-decoration-none"); // hack
            anchor->addStyleClass(std::string{ cssAnchorClass }); // hack
            result->addWidget(std::move(anchor));
            currentOffset = pos + artist->getName().size();
            matchCount += 1;
        }

        if (matchCount != artistIds.size())
            return createArtistAnchorList(artistIds, cssAnchorClass);

        return result;
    }

    std::unique_ptr<Wt::WContainerWidget> createArtistsAnchorsForRelease(db::ObjectPtr<db::Release> release, db::ArtistId omitIfMatchThisArtist, std::string_view cssAnchorClass)
    {
        using namespace db;

        Artist::FindParameters params;
        params.setRelease(release->getId());
        params.setLinkType(TrackArtistLinkType::ReleaseArtist);

        if (const auto releaseArtists{ Artist::findIds(LmsApp->getDbSession(), params) }; !releaseArtists.results.empty())
        {
            if (releaseArtists.results.size() == 1 && releaseArtists.results.front() == omitIfMatchThisArtist)
                return {};

            return createArtistDisplayNameWithAnchors(release->getArtistDisplayName(), releaseArtists.results, cssAnchorClass);
        }

        params.setLinkType(TrackArtistLinkType::Artist);
        const auto artists{ Artist::findIds(LmsApp->getDbSession(), params) };
        if (artists.results.size() == 1)
        {
            if (artists.results.front() == omitIfMatchThisArtist)
                return {};

            return createArtistAnchorList({ artists.results.front() }, cssAnchorClass);
        }

        if (artists.results.size() > 1)
        {
            auto res{ std::make_unique<Wt::WContainerWidget>() };
            res->addNew<Wt::WText>(Wt::WString::tr("Lms.Explore.various-artists"));
            return res;
        }

        return {};
    }

    Wt::WLink createArtistLink(db::Artist::pointer artist)
    {
        if (const auto mbid{ artist->getMBID() })
            return Wt::WLink{ Wt::LinkType::InternalPath, "/artist/mbid/" + std::string {mbid->getAsString()} };
        else
            return Wt::WLink{ Wt::LinkType::InternalPath, "/artist/" + artist->getId().toString() };
    }

    std::unique_ptr<Wt::WAnchor> createArtistAnchor(db::Artist::pointer artist, bool setText)
    {
        auto res = std::make_unique<Wt::WAnchor>(createArtistLink(artist));

        if (setText)
        {
            res->setTextFormat(Wt::TextFormat::Plain);
            res->setText(Wt::WString::fromUTF8(artist->getName()));
            res->setToolTip(Wt::WString::fromUTF8(artist->getName()), Wt::TextFormat::Plain);
        }

        return res;
    }

    Wt::WLink createReleaseLink(db::Release::pointer release)
    {
        if (const auto mbid{ release->getMBID() })
            return Wt::WLink{ Wt::LinkType::InternalPath, "/release/mbid/" + std::string {mbid->getAsString()} };
        else
            return Wt::WLink{ Wt::LinkType::InternalPath, "/release/" + release->getId().toString() };
    }

    std::unique_ptr<Wt::WAnchor> createReleaseAnchor(db::Release::pointer release, bool setText)
    {
        auto res = std::make_unique<Wt::WAnchor>(createReleaseLink(release));

        if (setText)
        {
            std::string releaseName{ release->getName() };
            res->setTextFormat(Wt::TextFormat::Plain);
            res->setText(Wt::WString::fromUTF8(releaseName));
            res->setToolTip(Wt::WString::fromUTF8(releaseName), Wt::TextFormat::Plain);
        }

        return res;
    }

    std::unique_ptr<Wt::WAnchor> createTrackListAnchor(db::TrackList::pointer trackList, bool setText)
    {
        Wt::WLink link{ Wt::LinkType::InternalPath, "/tracklist/" + trackList->getId().toString() };
        auto res{ std::make_unique<Wt::WAnchor>(link) };

        if (setText)
        {
            const Wt::WString name{ Wt::WString::fromUTF8(std::string {trackList->getName()}) };
            res->setTextFormat(Wt::TextFormat::Plain);
            res->setText(name);
            res->setToolTip(name, Wt::TextFormat::Plain);
        }

        return res;
    }

    std::unique_ptr<Wt::WContainerWidget> createClustersForTrack(db::Track::pointer track, Filters& filters)
    {
        using namespace db;

        std::unique_ptr<Wt::WContainerWidget> clusterContainer{ std::make_unique<Wt::WContainerWidget>() };

        // TODO: optimize this
        const auto clusterTypes{ ClusterType::findIds(LmsApp->getDbSession()).results };
        const auto clusterGroups{ track->getClusterGroups(clusterTypes, 3) };

        for (const auto& clusters : clusterGroups)
        {
            for (const Cluster::pointer& cluster : clusters)
            {
                const ClusterId clusterId{ cluster->getId() };
                Wt::WInteractWidget* entry{ clusterContainer->addWidget(createCluster(clusterId)) };
                entry->clicked().connect([&filters, clusterId]
                    {
                        filters.add(clusterId);
                    });
            }
        }

        return clusterContainer;
    }
}
