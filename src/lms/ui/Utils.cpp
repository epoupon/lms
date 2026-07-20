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

#include <algorithm>
#include <iomanip>
#include <sstream>

#include <Wt/WAnchor.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WText.h>

#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Movement.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/ScanSettings.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/Work.hpp"

#include "LmsApplication.hpp"
#include "ModalManager.hpp"
#include "explore/Filters.hpp"

namespace lms::ui::utils
{
    namespace
    {
        std::unique_ptr<Wt::WImage> createArtworkImage()
        {
            auto image{ std::make_unique<Wt::WImage>() };
            image->setStyleClass("Lms-cover img-fluid");                                          // HACK
            image->setAttributeValue("onload", LmsApp->javaScriptClass() + ".onLoadCover(this)"); // HACK
            image->setAlternateText(Wt::WString::tr("Lms.Explore.cover-art"));

            return image;
        }

        Wt::WLink createArtistLink(const db::Artist::pointer& artist)
        {
            if (const auto mbid{ artist->getMBID() })
                return Wt::WLink{ Wt::LinkType::InternalPath, "/artist/mbid/" + mbid->toString() };
            else
                return Wt::WLink{ Wt::LinkType::InternalPath, "/artist/" + artist->getId().toString() };
        }

        std::unique_ptr<Wt::WAnchor> createArtistAnchor(const db::Artist::pointer& artist, std::string_view displayName, bool setText)
        {
            auto res{ std::make_unique<Wt::WAnchor>(createArtistLink(artist)) };

            if (setText)
            {
                res->setTextFormat(Wt::TextFormat::Plain);
                res->setText(Wt::WString::fromUTF8(std::string{ displayName }));
                res->setToolTip(Wt::WString::fromUTF8(std::string{ artist->getName() }), Wt::TextFormat::Plain);
            }

            return res;
        }
    } // namespace

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

    void showArtworkModal(Wt::WLink image)
    {
        auto rawImage{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.template.full-modal-artwork")) };
        rawImage->bindNew<Wt::WImage>("artwork", image);

        Wt::WTemplate* rawImagePtr{ rawImage.get() };
        rawImage->clicked().connect([=] {
            LmsApp->getModalManager().dispose(rawImagePtr);
        });
        LmsApp->getModalManager().show(std::move(rawImage));
    }

    std::unique_ptr<Wt::WImage> createArtworkImage(db::ArtworkId artworkId, ArtworkResource::DefaultArtworkType type, ArtworkResource::Size size)
    {
        auto image{ createArtworkImage() };
        image->setImageLink(LmsApp->getArtworkResource()->getArtworkUrl(artworkId, type, size));
        return image;
    }

    std::unique_ptr<Wt::WImage> createDefaultArtworkImage(ArtworkResource::DefaultArtworkType type)
    {
        auto image{ createArtworkImage() };
        image->setImageLink(LmsApp->getArtworkResource()->getDefaultArtworkUrl(type));
        return image;
    }

    std::unique_ptr<Wt::WInteractWidget> createFilter(const Wt::WString& name, const Wt::WString& tooltip, std::string_view colorStyleClass, bool canDelete)
    {
        auto res{ std::make_unique<Wt::WContainerWidget>() };
        res->setInline(true);
        res->setStyleClass("Lms-badge-cluster badge me-1 " + std::string{ colorStyleClass }); // HACK
        res->setToolTip(tooltip, Wt::TextFormat::Plain);

        if (canDelete)
            res->addNew<Wt::WText>("<i class=\"fa fa-times-circle\"></i> ", Wt::TextFormat::XHTML);

        res->addNew<Wt::WText>(name, Wt::TextFormat::Plain);

        return res;
    }

    std::unique_ptr<Wt::WInteractWidget> createFilterCluster(db::ClusterId clusterId, bool canDelete)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Cluster::pointer cluster{ db::Cluster::find(LmsApp->getDbSession(), clusterId) };
        if (!cluster)
            return {};

        auto getStyleClass{ [](const db::Cluster::pointer& cluster) -> std::string_view {
            switch (cluster->getType()->getId().getValue() % 8)
            {
            case 0:
                return "bg-primary";
            case 1:
                return "bg-secondary";
            case 2:
                return "bg-success";
            case 3:
                return "bg-danger";
            case 4:
                return "bg-warning text-dark";
            case 5:
                return "bg-info text-dark";
            case 6:
                return "bg-light text-dark";
            case 7:
                return "bg-dark";
            }

            return "bg-primary";
        } };

        return createFilter(Wt::WString::fromUTF8(std::string{ cluster->getName() }), Wt::WString::fromUTF8(std::string{ cluster->getType()->getName() }), getStyleClass(cluster), canDelete);
    }

    std::unique_ptr<Wt::WInteractWidget> createFilterGenre(db::GenreId genreId, bool canDelete)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Genre::pointer genre{ db::Genre::find(LmsApp->getDbSession(), genreId) };
        if (!genre)
            return {};

        return createFilter(Wt::WString::fromUTF8(std::string{ genre->getName() }), Wt::WString::trn("Lms.Explore.genre", 1), "bg-info text-dark", canDelete);
    }

    std::unique_ptr<Wt::WInteractWidget> createFilterGrouping(db::GroupingId groupingId, bool canDelete)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Grouping::pointer grouping{ db::Grouping::find(LmsApp->getDbSession(), groupingId) };
        if (!grouping)
            return {};

        return createFilter(Wt::WString::fromUTF8(std::string{ grouping->getName() }), Wt::WString::trn("Lms.Explore.grouping", 1), "bg-secondary", canDelete);
    }

    std::unique_ptr<Wt::WInteractWidget> createFilterLanguage(db::LanguageId languageId, bool canDelete)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Language::pointer language{ db::Language::find(LmsApp->getDbSession(), languageId) };
        if (!language)
            return {};

        return createFilter(Wt::WString::fromUTF8(std::string{ language->getName() }), Wt::WString::trn("Lms.Explore.language", 1), "bg-primary", canDelete);
    }

    std::unique_ptr<Wt::WInteractWidget> createFilterMood(db::MoodId moodId, bool canDelete)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Mood::pointer mood{ db::Mood::find(LmsApp->getDbSession(), moodId) };
        if (!mood)
            return {};

        return createFilter(Wt::WString::fromUTF8(std::string{ mood->getName() }), Wt::WString::trn("Lms.Explore.mood", 1), "bg-warning text-dark", canDelete);
    }

    std::unique_ptr<Wt::WContainerWidget> createFilterClustersForTrack(db::Track::pointer track, Filters& filters)
    {
        using namespace db;

        std::unique_ptr<Wt::WContainerWidget> clusterContainer{ std::make_unique<Wt::WContainerWidget>() };

        // TODO: optimize this
        const auto clusterTypes{ ClusterType::findIds(LmsApp->getDbSession()) };
        const auto clusterGroups{ track->getClusterGroups(clusterTypes, 3) };

        for (const auto& clusters : clusterGroups)
        {
            for (const Cluster::pointer& cluster : clusters)
            {
                const ClusterId clusterId{ cluster->getId() };
                Wt::WInteractWidget* entry{ clusterContainer->addWidget(createFilterCluster(clusterId)) };
                entry->clicked().connect([&filters, clusterId] {
                    filters.add(clusterId);
                });
            }
        }

        return clusterContainer;
    }

    std::unique_ptr<Wt::WContainerWidget> createArtistAnchorList(const std::vector<db::Artist::pointer>& artists, std::string_view cssAnchorClass)
    {
        std::unique_ptr<Wt::WContainerWidget> artistContainer{ std::make_unique<Wt::WContainerWidget>() };

        bool firstArtist{ true };

        for (const auto& artist : artists)
        {
            if (!firstArtist)
                artistContainer->addNew<Wt::WText>(" · ");
            firstArtist = false;

            auto anchor{ createArtistAnchor(artist) };
            anchor->addStyleClass("text-decoration-none"); // hack
            anchor->addStyleClass(std::string{ cssAnchorClass });
            artistContainer->addWidget(std::move(anchor));
        }

        return artistContainer;
    }

    std::unique_ptr<Wt::WAnchor> createArtistAnchor(db::Artist::pointer artist, bool setText)
    {
        return createArtistAnchor(artist, artist->getName(), setText);
    }

    Wt::WLink createReleaseLink(db::Release::pointer release)
    {
        if (const auto mbid{ release->getMBID() })
            return Wt::WLink{ Wt::LinkType::InternalPath, "/release/mbid/" + mbid->toString() };

        return Wt::WLink{ Wt::LinkType::InternalPath, "/release/" + release->getId().toString() };
    }

    std::unique_ptr<Wt::WAnchor> createReleaseAnchor(db::Release::pointer release, bool setText)
    {
        auto res = std::make_unique<Wt::WAnchor>(createReleaseLink(release));
        if (setText)
        {
            std::string releaseName{ release->getName() };
            if (std::string_view releaseComment{ release->getComment() }; !releaseComment.empty())
            {
                releaseName += " [";
                releaseName += releaseComment;
                releaseName += ']';
            }

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
            const Wt::WString name{ Wt::WString::fromUTF8(std::string{ trackList->getName() }) };
            res->setTextFormat(Wt::TextFormat::Plain);
            res->setText(name);
            res->setToolTip(name, Wt::TextFormat::Plain);
        }

        return res;
    }

    ArtistDisplayInfo computeArtistDisplayInfo(db::ObjectPtr<db::Release> release)
    {
        ArtistDisplayInfo res;

        res.displayName = release->getArtistDisplayName();
        release->visitArtistLinks([&res](const db::ObjectPtr<db::ReleaseArtistLink>& artistLink) {
            res.entries.emplace_back(ArtistDisplayInfo::Entry{ .displayName = std::string{ artistLink->getArtistName() }, .artist = artistLink->getArtist() });
        });

        // If no release artists, fallback on track artists, only if there are only 1 artist, otherwise just put various-artists
        if (res.entries.empty())
        {
            res.displayName.clear();

            const auto trackArtists{ release->getTrackArtists(db::TrackArtistLinkType::Artist) };

            if (trackArtists.size() > 1)
                res.displayName = Wt::WString::tr("Lms.Explore.various-artists").toUTF8();
            else if (trackArtists.size() == 1)
                res.entries.emplace_back(ArtistDisplayInfo::Entry{ .displayName = std::string{ trackArtists[0]->getName() }, .artist = trackArtists[0] });
        }

        return res;
    }

    ArtistDisplayInfo computeArtistDisplayInfo(db::ObjectPtr<db::Track> track, db::TrackArtistLinkType linkType)
    {
        ArtistDisplayInfo res;

        res.displayName = track->getArtistDisplayName();
        track->visitArtistLinks(linkType, [&res](const db::ObjectPtr<db::TrackArtistLink>& artistLink) {
            res.entries.emplace_back(ArtistDisplayInfo::Entry{ .displayName = std::string{ artistLink->getArtistName() }, .artist = artistLink->getArtist() });
        });

        return res;
    }

    std::unique_ptr<Wt::WContainerWidget> createArtistsAnchors(const ArtistDisplayInfo& artistDisplayInfo, std::string_view cssAnchorClass)
    {
        auto result{ std::make_unique<Wt::WContainerWidget>() };
        result->setInline(true); // TODO: use a template for that?

        std::size_t matchCount{};
        std::string_view::size_type currentOffset{};

        if (artistDisplayInfo.entries.empty())
        {
            if (!artistDisplayInfo.displayName.empty())
                result->addNew<Wt::WText>(std::string{ artistDisplayInfo.displayName }, Wt::TextFormat::Plain);

            return result;
        }

        // consider order is guaranteed + we will likely succeed
        for (const ArtistDisplayInfo::Entry& entry : artistDisplayInfo.entries)
        {
            const auto pos{ artistDisplayInfo.displayName.find(entry.displayName, currentOffset) };
            if (pos == std::string_view::npos)
                break;

            assert(pos >= currentOffset);
            if (pos != currentOffset)
                result->addNew<Wt::WText>(std::string{ artistDisplayInfo.displayName.substr(currentOffset, pos - currentOffset) }, Wt::TextFormat::Plain);

            auto anchor{ createArtistAnchor(entry.artist, entry.displayName, true) };
            anchor->addStyleClass("text-decoration-none");        // hack
            anchor->addStyleClass(std::string{ cssAnchorClass }); // hack
            result->addWidget(std::move(anchor));

            currentOffset = pos + entry.displayName.size();
            matchCount += 1;
        }

        if (matchCount == artistDisplayInfo.entries.size())
        {
            const std::string_view remainingStr{ artistDisplayInfo.displayName.substr(currentOffset) };
            if (!remainingStr.empty())
                result->addNew<Wt::WText>(std::string{ remainingStr }, Wt::TextFormat::Plain);
        }
        else
        {
            std::vector<db::Artist::pointer> artists;
            std::transform(std::cbegin(artistDisplayInfo.entries), std::cend(artistDisplayInfo.entries), std::back_inserter(artists), [](const ArtistDisplayInfo::Entry& entry) {
                return entry.artist;
            });

            result = createArtistAnchorList(artists, cssAnchorClass);
        }

        return result;
    }

    std::unique_ptr<Wt::WContainerWidget> createArtistsAnchors(db::ObjectPtr<db::Track> track, db::TrackArtistLinkType linkType, std::string_view cssAnchorClass)
    {
        const auto computeArtistDisplayInfo{ utils::computeArtistDisplayInfo(track, linkType) };
        return utils::createArtistsAnchors(computeArtistDisplayInfo, cssAnchorClass);
    }

    std::unique_ptr<Wt::WContainerWidget> createArtistsAnchors(db::ObjectPtr<db::Release> release, std::string_view cssAnchorClass)
    {
        const auto computeArtistDisplayInfo{ utils::computeArtistDisplayInfo(release) };
        return utils::createArtistsAnchors(computeArtistDisplayInfo, cssAnchorClass);
    }

    std::map<Wt::WString, std::vector<db::ObjectPtr<db::Artist>>> getArtistsByRole(db::ObjectPtr<db::Track> track, core::EnumSet<db::TrackArtistLinkType> artistLinkTypes)
    {
        std::map<Wt::WString, std::vector<db::Artist::pointer>> artistMap;

        auto addArtists = [&](db::TrackArtistLinkType linkType, const char* type) {
            if (!artistLinkTypes.empty() && !artistLinkTypes.contains(linkType))
                return;

            std::vector<db::Artist::pointer> artists;
            track->visitArtistLinks(linkType, [&](const auto& link) {
                artists.push_back(link->getArtist());
            });

            if (artists.empty())
                return;

            Wt::WString typeStr{ Wt::WString::trn(type, artists.size()) };
            artistMap[typeStr] = std::move(artists);
        };

        std::vector<db::ObjectPtr<db::Artist>> rolelessPerformers;
        auto addPerformerArtists = [&] {
            if (!artistLinkTypes.empty() && !artistLinkTypes.contains(db::TrackArtistLinkType::Performer))
                return;

            track->visitArtistLinks(db::TrackArtistLinkType::Performer, [&](const auto& link) {
                if (link->getSubType().empty())
                    rolelessPerformers.push_back(link->getArtist());
                else
                    artistMap[std::string{ link->getSubType() }].push_back(link->getArtist());
            });
        };

        addArtists(db::TrackArtistLinkType::Composer, "Lms.Explore.composer");
        addArtists(db::TrackArtistLinkType::Conductor, "Lms.Explore.conductor");
        addArtists(db::TrackArtistLinkType::Lyricist, "Lms.Explore.lyricist");
        addArtists(db::TrackArtistLinkType::Mixer, "Lms.Explore.mixer");
        addArtists(db::TrackArtistLinkType::Remixer, "Lms.Explore.remixer");
        addArtists(db::TrackArtistLinkType::Producer, "Lms.Explore.producer");
        addPerformerArtists();

        if (!rolelessPerformers.empty())
        {
            Wt::WString performersStr{ Wt::WString::trn("Lms.Explore.performer", rolelessPerformers.size()) };
            artistMap[performersStr] = std::move(rolelessPerformers);
        }

        return artistMap;
    }

    std::map<Wt::WString, std::vector<db::ObjectPtr<db::Artist>>> getTrackArtistsByRole(db::ObjectPtr<db::Release> release)
    {
        std::map<Wt::WString, std::vector<db::Artist::pointer>> artistMap;

        auto addArtists = [&](db::TrackArtistLinkType linkType, const char* type) {
            std::vector<db::Artist::pointer> artists;
            release->visitTrackArtists(linkType, [&](const auto& artist) {
                artists.push_back(artist);
            });

            if (artists.empty())
                return;

            Wt::WString typeStr{ Wt::WString::trn(type, artists.size()) };
            artistMap[typeStr] = std::move(artists);
        };

        std::vector<db::ObjectPtr<db::Artist>> rolelessPerformers;
        auto addPerformerArtists = [&] {
            release->visitTrackArtistLinks(db::TrackArtistLinkType::Performer, [&](const db::TrackArtistLink::pointer& link) {
                if (link->getSubType().empty())
                {
                    if (std::find(std::cbegin(rolelessPerformers), std::cend(rolelessPerformers), link->getArtist()) == std::cend(rolelessPerformers))
                        rolelessPerformers.push_back(link->getArtist());
                }
                else
                {
                    auto& artists{ artistMap[std::string{ link->getSubType() }] };
                    if (std::find(std::cbegin(artists), std::cend(artists), link->getArtist()) == std::cend(artists))
                        artists.push_back(link->getArtist());
                }
            });
        };

        addArtists(db::TrackArtistLinkType::Composer, "Lms.Explore.composer");
        addArtists(db::TrackArtistLinkType::Conductor, "Lms.Explore.conductor");
        addArtists(db::TrackArtistLinkType::Lyricist, "Lms.Explore.lyricist");
        addArtists(db::TrackArtistLinkType::Mixer, "Lms.Explore.mixer");
        addArtists(db::TrackArtistLinkType::Remixer, "Lms.Explore.remixer");
        addArtists(db::TrackArtistLinkType::Producer, "Lms.Explore.producer");
        addPerformerArtists();

        if (!rolelessPerformers.empty())
        {
            Wt::WString performersStr{ Wt::WString::trn("Lms.Explore.performer", rolelessPerformers.size()) };
            artistMap[performersStr] = std::move(rolelessPerformers);
        }

        return artistMap;
    }

    std::unique_ptr<Wt::WInteractWidget> createCopyright(std::string_view copyright, std::string_view copyrightURL)
    {
        std::unique_ptr<Wt::WInteractWidget> res;

        if (copyright.empty() && copyrightURL.empty())
            return res;

        if (copyright.empty() && !copyrightURL.empty())
            copyright = copyrightURL;

        if (!copyrightURL.empty())
        {
            Wt::WLink link{ std::string{ copyrightURL } };
            link.setTarget(Wt::LinkTarget::NewWindow);

            auto anchor{ std::make_unique<Wt::WAnchor>(link) };
            anchor->setTextFormat(Wt::TextFormat::Plain);
            anchor->setText(Wt::WString::fromUTF8(std::string{ copyright }));

            res = std::move(anchor);
        }
        else
            res = std::make_unique<Wt::WText>(Wt::WString::fromUTF8(std::string{ copyright }), Wt::TextFormat::Plain);

        return res;
    }

    void copyToClipboard(std::string_view text)
    {
        LmsApp->doJavaScript("navigator.clipboard.writeText('" + core::stringUtils::jsEscape(text) + "').catch(function(){})");
    }

    TrackDisplayInfo computeTrackDisplayInfo(const db::ObjectPtr<db::Track>& track)
    {
        const auto movements{ track->getMovements() };
        if (!movements.empty() && track->hasWork())
        {
            const db::Movement::pointer& movement{ movements.front() };
            std::string title;
            if (const auto n{ movement->getNumber() })
            {
                if (const std::string numeral{ core::stringUtils::toRomanNumeral(*n) }; !numeral.empty())
                    title += numeral + ". ";
            }

            title += movement->getName();

            if (!title.empty())
            {
                TrackDisplayInfo info{ .title = std::move(title), .workName = std::nullopt };
                if (const auto works{ track->getWorks() }; !works.empty())
                    info.workName = std::string{ works.front()->getName() };

                return info;
            }
        }

        return TrackDisplayInfo{ .title = std::string{ track->getName() }, .workName = std::nullopt };
    }
} // namespace lms::ui::utils
