/*
 * Copyright (C) 2014 Emeric Poupon
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

#include "DownloadResource.hpp"

#include <Wt/Http/Response.h>
#include <Wt/WDateTime.h>

#include "core/ILogger.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackList.hpp"

#include "LmsApplication.hpp"

#define DL_RESOURCE_LOG(severity, message) LMS_LOG(UI, severity, "Download resource: " << message)

namespace lms::ui
{
    DownloadResource::~DownloadResource()
    {
        beingDeleted();
    }

    void DownloadResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
    {
        try
        {
            std::shared_ptr<zip::IZipper> zipper;

            // First, see if this request is for a continuation
            if (Wt::Http::ResponseContinuation * continuation{ request.continuation() })
                zipper = Wt::cpp17::any_cast<std::shared_ptr<zip::IZipper>>(continuation->data());
            else
            {
                zipper = createZipper();
                if (!zipper)
                {
                    // no track, may be a legit case
                    response.setStatus(404);
                    return;
                }

                response.setMimeType("application/zip");
            }

            zipper->writeSome(response.out());
            if (!zipper->isComplete())
            {
                auto* continuation{ response.createContinuation() };
                continuation->setData(zipper);
            }
        }
        catch (zip::Exception& exception)
        {
            DL_RESOURCE_LOG(ERROR, "Zipper exception: " << exception.what());
        }
    }

    namespace
    {
        std::string getArtistPathName(db::Artist::pointer artist)
        {
            return core::stringUtils::replaceInString(artist->getName(), "/", "_");
        }

        std::string getReleaseArtistPathName(db::Release::pointer release)
        {
            std::string releaseArtistName;

            std::vector<db::ObjectPtr<db::Artist>> artists;

            artists = release->getReleaseArtists();
            if (artists.empty())
                artists = release->getArtists();

            if (artists.size() > 1)
                releaseArtistName = Wt::WString::tr("Lms.Explore.various-artists").toUTF8();
            else if (artists.size() == 1)
                releaseArtistName = artists.front()->getName();

            releaseArtistName = core::stringUtils::replaceInString(releaseArtistName, "/", "_");

            return releaseArtistName;
        }

        std::string getReleasePathName(db::Release::pointer release)
        {
            std::string releaseName;

            if (const auto year{ release->getYear() })
                releaseName += std::to_string(*year) + " - ";
            releaseName += core::stringUtils::replaceInString(release->getName(), "/", "_");

            return releaseName;
        }
    } // namespace

    namespace details
    {
        std::string getTrackPathName(db::Track::pointer track)
        {
            std::ostringstream fileName;

            if (auto discNumber{ track->getDiscNumber() })
                fileName << *discNumber << ".";
            if (auto trackNumber{ track->getTrackNumber() })
                fileName << std::setw(2) << std::setfill('0') << *trackNumber << " - ";

            fileName << core::stringUtils::replaceInString(track->getName(), "/", "_") << track->getAbsoluteFilePath().extension().string();

            return fileName.str();
        }

        std::string getTrackListPathName(db::TrackList::pointer trackList)
        {
            return core::stringUtils::replaceInString(trackList->getName(), "/", "_");
        }

        std::unique_ptr<zip::IZipper> createZipper(const std::vector<db::Track::pointer>& tracks)
        {
            if (tracks.empty())
                return {};

            zip::EntryContainer files;
            for (const db::Track::pointer& track : tracks)
            {
                std::string releaseName;
                std::string releaseArtistName;
                if (auto release{ track->getRelease() })
                {
                    releaseName = getReleasePathName(release);
                    releaseArtistName = getReleaseArtistPathName(release);
                }

                std::string fileName;
                if (!releaseArtistName.empty())
                    fileName += releaseArtistName + "/";
                if (!releaseName.empty())
                    fileName += releaseName + "/";
                fileName += getTrackPathName(track);

                files.emplace_back(zip::Entry{ fileName, track->getAbsoluteFilePath() });
            }

            return zip::createArchiveZipper(files);
        }
    } // namespace details

    DownloadArtistResource::DownloadArtistResource(db::ArtistId artistId)
        : _artistId{ artistId }
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        db::Artist::pointer artist{ db::Artist::find(LmsApp->getDbSession(), artistId) };
        if (artist)
            suggestFileName(getArtistPathName(artist) + ".zip");
    }

    std::unique_ptr<zip::IZipper> DownloadArtistResource::createZipper()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const auto trackResults{ db::Track::find(LmsApp->getDbSession(), db::Track::FindParameters{}.setArtist(_artistId).setSortMethod(db::TrackSortMethod::DateDescAndRelease)) };
        return details::createZipper(trackResults.results);
    }

    DownloadReleaseResource::DownloadReleaseResource(db::ReleaseId releaseId)
        : _releaseId{ releaseId }
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        db::Release::pointer release{ db::Release::find(LmsApp->getDbSession(), releaseId) };
        if (release)
            suggestFileName(getReleasePathName(release) + ".zip");
    }

    std::unique_ptr<zip::IZipper> DownloadReleaseResource::createZipper()
    {
        using namespace db;

        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        auto tracks{ Track::find(LmsApp->getDbSession(), Track::FindParameters{}.setRelease(_releaseId).setSortMethod(TrackSortMethod::Release)) };
        return details::createZipper(tracks.results);
    }

    DownloadTrackResource::DownloadTrackResource(db::TrackId trackId)
        : _trackId{ trackId }
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        db::Track::pointer track{ db::Track::find(LmsApp->getDbSession(), trackId) };
        if (track)
            suggestFileName(details::getTrackPathName(track) + ".zip");
    }

    std::unique_ptr<zip::IZipper> DownloadTrackResource::createZipper()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Track::pointer track{ db::Track::find(LmsApp->getDbSession(), _trackId) };
        if (!track)
        {
            DL_RESOURCE_LOG(DEBUG, "Cannot find track");
            return {};
        }

        return details::createZipper({ track });
    }

    DownloadTrackListResource::DownloadTrackListResource(db::TrackListId trackListId)
        : _trackListId{ trackListId }
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::TrackList::pointer trackList{ db::TrackList::find(LmsApp->getDbSession(), trackListId) };
        if (trackList)
            suggestFileName(details::getTrackListPathName(trackList) + ".zip");
    }

    std::unique_ptr<zip::IZipper> DownloadTrackListResource::createZipper()
    {
        using namespace db;
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        Track::FindParameters params;
        params.setTrackList(_trackListId);
        const auto tracks{ Track::find(LmsApp->getDbSession(), params) };
        return details::createZipper(tracks.results);
    }
} // namespace lms::ui
