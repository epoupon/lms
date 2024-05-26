/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "ReleaseHelpers.hpp"

#include <LmsApplication.hpp>
#include <ModalManager.hpp>
#include <Wt/WAnchor.h>
#include <Wt/WImage.h>
#include <Wt/WText.h>
#include <av/IAudioFile.hpp>
#include <common/Template.hpp>
#include <database/Session.hpp>
#include <database/Track.hpp>
#include <database/TrackArtistLink.hpp>
#include <services/scrobbling/IScrobblingService.hpp>

#include "database/Artist.hpp"
#include "database/Release.hpp"

#include "Utils.hpp"

namespace lms::ui::releaseListHelpers
{
    using namespace db;

    namespace
    {
        std::unique_ptr<Wt::WTemplate> createEntryInternal(const Release::pointer& release, const std::string& templateKey, const Artist::pointer& artist, const bool showYear)
        {
            auto entry{ std::make_unique<Wt::WTemplate>(Wt::WString::tr(templateKey)) };

            entry->bindWidget("release-name", utils::createReleaseAnchor(release));
            entry->addFunction("tr", &Wt::WTemplate::Functions::tr);

            {
                Wt::WAnchor* anchor{ entry->bindWidget("cover", utils::createReleaseAnchor(release, false)) };
                auto cover{ utils::createCover(release->getId(), CoverResource::Size::Large) };
                cover->addStyleClass("Lms-cover-release Lms-cover-anchor");
                anchor->setImage(std::move(cover));
            }

            auto artistAnchors{ utils::createArtistsAnchorsForRelease(release, artist ? artist->getId() : ArtistId{}, "link-secondary") };
            if (artistAnchors)
            {
                entry->setCondition("if-has-artist", true);
                entry->bindWidget("artist-name", std::move(artistAnchors));
            }

            if (showYear)
            {
                Wt::WString year{ releaseHelpers::buildReleaseYearString(release->getYear(), release->getOriginalYear()) };
                if (!year.empty())
                {
                    entry->setCondition("if-has-year", true);
                    entry->bindString("year", year, Wt::TextFormat::Plain);
                }
            }

            return entry;
        }
    } // namespace

    std::unique_ptr<Wt::WTemplate> createEntry(const Release::pointer& release, const Artist::pointer& artist, bool showYear)
    {
        return createEntryInternal(release, "Lms.Explore.Releases.template.entry-grid", artist, showYear);
    }

    std::unique_ptr<Wt::WTemplate> createEntry(const Release::pointer& release)
    {
        return createEntry(release, Artist::pointer{}, false /*year*/);
    }

    std::unique_ptr<Wt::WTemplate> createEntryForArtist(const db::Release::pointer& release, const db::Artist::pointer& artist)
    {
        return createEntry(release, artist, true);
    }
} // namespace lms::ui::releaseListHelpers

namespace lms::ui::releaseHelpers
{
    Wt::WString buildReleaseTypeString(const ReleaseType& releaseType)
    {
        Wt::WString res;

        if (releaseType.primaryType)
        {
            switch (*releaseType.primaryType)
            {
            case PrimaryReleaseType::Album:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-album");
                break;
            case PrimaryReleaseType::Broadcast:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-broadcast");
                break;
            case PrimaryReleaseType::EP:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-ep");
                break;
            case PrimaryReleaseType::Single:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-single");
                break;
            case PrimaryReleaseType::Other:
                res = Wt::WString::tr("Lms.Explore.Release.type-primary-other");
                break;
            }
        }

        for (SecondaryReleaseType secondaryType : releaseType.secondaryTypes)
        {
            if (!res.empty())
                res += Wt::WString{ " · " };

            switch (secondaryType)
            {
            case SecondaryReleaseType::Compilation:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-compilation");
                break;
            case SecondaryReleaseType::Spokenword:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-spokenword");
                break;
            case SecondaryReleaseType::Soundtrack:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-soundtrack");
                break;
            case SecondaryReleaseType::Interview:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-interview");
                break;
            case SecondaryReleaseType::Audiobook:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-audiobook");
                break;
            case SecondaryReleaseType::AudioDrama:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-audiodrama");
                break;
            case SecondaryReleaseType::Live:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-live");
                break;
            case SecondaryReleaseType::Remix:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-remix");
                break;
            case SecondaryReleaseType::DJMix:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-djmix");
                break;
            case SecondaryReleaseType::Mixtape_Street:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-mixtape-street");
                break;
            case SecondaryReleaseType::Demo:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-demo");
                break;
            case SecondaryReleaseType::FieldRecording:
                res += Wt::WString::tr("Lms.Explore.Release.type-secondary-field-recording");
                break;
            }
        }

        for (const std::string& customType : releaseType.customTypes)
        {
            if (!res.empty())
                res += Wt::WString{ " · " };

            res += customType;
        }

        return res;
    }

    Wt::WString buildReleaseYearString(std::optional<int> year, std::optional<int> originalYear)
    {
        Wt::WString res;

        // Year can be here, but originalYear can't be here without year (enforced by scanner)
        if (!year)
            return res;

        if (originalYear && *originalYear != *year)
            res = std::to_string(*originalYear) + " (" + std::to_string(*year) + ")";
        else
            res = std::to_string(*year);

        return res;
    }

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

        std::map<Wt::WString, std::set<db::ArtistId>> artistMap;

        auto addArtists = [&](db::TrackArtistLinkType linkType, const char* type) {
            db::Artist::FindParameters params;
            params.setRelease(releaseId);
            params.setLinkType(linkType);
            const auto artistIds{ db::Artist::findIds(LmsApp->getDbSession(), params) };
            if (artistIds.results.empty())
                return;

            Wt::WString typeStr{ Wt::WString::trn(type, artistIds.results.size()) };
            for (db::ArtistId artistId : artistIds.results)
                artistMap[typeStr].insert(artistId);
        };

        auto addPerformerArtists = [&] {
            db::TrackArtistLink::FindParameters params;
            params.setRelease(releaseId);
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
        for (const db::Track::pointer& track : db::Track::find(LmsApp->getDbSession(), db::Track::FindParameters{}.setRelease(releaseId).setRange(db::Range{ 0, 1 })).results)
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
} // namespace lms::ui::releaseHelpers
