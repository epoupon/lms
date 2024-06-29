/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "ScanStepScanArtistImages.hpp"

#include <array>
#include <cassert>
#include <deque>
#include <set>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Path.hpp"
#include "database/Artist.hpp"
#include "database/Db.hpp"
#include "database/Image.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "image/Exception.hpp"
#include "image/Image.hpp"

namespace lms::scanner
{
    namespace
    {
        constexpr std::size_t readBatchSize{ 10 };
        constexpr std::size_t writeBatchSize{ 5 };

        struct ImageInfo
        {
            operator bool() const { return !imagePath.empty(); }
            void clear()
            {
                imagePath.clear();
                lastWriteTime = {};
                fileSize = {};
                height = {};
                width = {};
            }

            std::filesystem::path imagePath;
            Wt::WDateTime lastWriteTime;
            std::size_t fileSize{};
            std::size_t height{};
            std::size_t width{};
        };

        bool tryDecodeImage(const std::filesystem::path& imagePath, ImageInfo& imageInfo)
        {
            assert(!imageInfo);

            try
            {
                std::unique_ptr<image::IRawImage> rawImage{ image::decodeImage(imagePath) };
                imageInfo.imagePath = imagePath;
                imageInfo.fileSize = std::filesystem::file_size(imagePath);
                imageInfo.width = rawImage->getWidth();
                imageInfo.height = rawImage->getHeight();
                imageInfo.lastWriteTime = core::pathUtils::getLastWriteTime(imagePath);
            }
            catch (const image::Exception& e)
            {
                LMS_LOG(DBUPDATER, ERROR, "Cannot read image in file '" << imagePath.string() << "': " << e.what());
                return false;
            }

            return true;
        }

        struct ArtistImageInfo
        {
            db::ArtistId artistId;
            ImageInfo imageInfo;
        };

        using ArtistImageInfoContainer = std::deque<ArtistImageInfo>;

        bool isFileSupported(const std::filesystem::path& file)
        {
            static const std::array<std::filesystem::path, 4> fileExtensions{ ".jpg", ".jpeg", ".png", ".bmp" }; // TODO parametrize

            return (std::find(std::cbegin(fileExtensions), std::cend(fileExtensions), file.extension()) != std::cend(fileExtensions));
        }

        std::multimap<std::string, std::filesystem::path> getImagePaths(const std::filesystem::path& directoryPath, const std::vector<std::string>& fileNames)
        {
            std::multimap<std::string, std::filesystem::path> res;
            std::error_code ec;

            std::filesystem::directory_iterator itPath(directoryPath, ec);
            const std::filesystem::directory_iterator itEnd;
            while (!ec && itPath != itEnd)
            {
                const std::filesystem::path& path{ *itPath };
                const std::string stem{ path.stem().string() };
                if (isFileSupported(path)
                    && std::any_of(std::cbegin(fileNames), std::cend(fileNames), [&](const std::string& fileName) { return core::stringUtils::stringCaseInsensitiveEqual(stem, fileName); }))
                {
                    res.emplace(stem, path);
                }

                itPath.increment(ec);
            }

            return res;
        }

        bool findImageInDirectory(const std::filesystem::path& directory, const std::vector<std::string>& fileNames, ImageInfo& imageInfo)
        {
            assert(!imageInfo);

            const std::multimap<std::string, std::filesystem::path> coverPaths{ getImagePaths(directory, fileNames) };

            for (const std::string_view fileName : fileNames)
            {
                const auto range{ coverPaths.equal_range(std::string{ fileName }) };
                for (auto it{ range.first }; it != range.second; ++it)
                {
                    if (tryDecodeImage(it->second, imageInfo))
                        return true;
                }
            }

            return false;
        }

        void fetchArtistImageInfo(db::Session& session, const std::vector<std::string>& genericArtistFileNames, const db::Artist::pointer& artist, ImageInfo& imageInfo)
        {
            const std::string artistMBID{ [&] {
                std::string artistMBID;
                if (auto mbid{ artist->getMBID() })
                    artistMBID = mbid->getAsString();
                return artistMBID;
            }() };

            std::set<std::filesystem::path> releasePaths;
            std::set<std::filesystem::path> multiArtistReleasePaths;

            db::Track::FindParameters params;
            params.setArtist(artist->getId(), { db::TrackArtistLinkType::ReleaseArtist });

            db::Track::find(session, params, [&](const db::Track::pointer& track) {
                db::Artist::FindParameters artistFindParams;
                artistFindParams.setTrack(track->getId());
                artistFindParams.setLinkType(db::TrackArtistLinkType::ReleaseArtist);

                const auto releaseArtists{ db::Artist::findIds(session, artistFindParams) };
                if (releaseArtists.results.size() == 1)
                    releasePaths.insert(track->getAbsoluteFilePath().parent_path());
                else
                    multiArtistReleasePaths.insert(track->getAbsoluteFilePath().parent_path());
            });

            std::vector<std::string> artistFileNames;
            if (!artistMBID.empty())
                artistFileNames.push_back(artistMBID);
            artistFileNames.push_back(artist->getName());

            std::vector<std::string> artistFileNamesWithGenericNames{ artistFileNames };
            artistFileNamesWithGenericNames.insert(artistFileNamesWithGenericNames.end(), std::cbegin(genericArtistFileNames), std::cend(genericArtistFileNames));

            // Expect layout like this:
            // ReleaseArtist/Release/Tracks'
            //              /artist-mbid.jpg
            //              /artist-name.jpg
            //              /artist.jpg
            if (!releasePaths.empty())
            {
                const std::filesystem::path artistPath{ releasePaths.size() == 1 ? releasePaths.begin()->parent_path() : core::pathUtils::getLongestCommonPath(std::cbegin(releasePaths), std::cend(releasePaths)) };
                if (findImageInDirectory(artistPath, artistFileNamesWithGenericNames, imageInfo))
                    return;
            }

            // Expect layout like this:
            // ReleaseArtist/Release/Tracks'
            //                      /artist-mbid.jpg
            //                      /artist-name.jpg
            //                      /artist.jpg
            for (const std::filesystem::path& releasePath : releasePaths)
            {
                // TODO: what if an artist has released an album that bears their name?
                if (findImageInDirectory(releasePath, artistFileNamesWithGenericNames, imageInfo))
                    return;
            }

            // Expect layout like this:
            // Only search for the artist's name in the release path, as we can't map a generic name to several artists
            // ReleaseArtist/Release/Tracks'
            //                      /artist-name.jpg
            //                      /artist-mbid.jpg
            for (const std::filesystem::path& releasePath : multiArtistReleasePaths)
            {
                if (findImageInDirectory(releasePath, artistFileNames, imageInfo))
                    return;
            }
        }

        bool artistImageNeedsUpdate(const db::Image::pointer& image, const ImageInfo& imageInfo)
        {
            if (!imageInfo && !image) // no image as before
                return false;
            else if (!imageInfo && image) // no longer has image
                return true;
            else if (imageInfo && !image) // image has been added
                return true;

            assert(imageInfo);
            // artist image still here, consider it is the same only if the last modified time is the same
            return imageInfo.lastWriteTime != image->getLastWriteTime();
        }

        struct SearchImageContext
        {
            db::Session& session;
            db::ArtistId lastRetrievedArtistId;
            const std::vector<std::string>& artistFileNames;
            bool fullScan;
        };

        bool fetchNextArtistImagesToUpdate(SearchImageContext& searchContext, ArtistImageInfoContainer& artistImageInfoList)
        {
            const db::ArtistId artistId{ searchContext.lastRetrievedArtistId };
            ImageInfo imageInfo;

            {
                auto transaction{ searchContext.session.createReadTransaction() };

                db::Artist::find(searchContext.session, searchContext.lastRetrievedArtistId, readBatchSize, [&](const db::Artist::pointer& artist) {
                    imageInfo.clear();

                    fetchArtistImageInfo(searchContext.session, searchContext.artistFileNames, artist, imageInfo);
                    if (imageInfo)
                        LMS_LOG(DBUPDATER, DEBUG, "Found artist image for artist '" << artist->getName() << "' at '" << imageInfo.imagePath << "'");

                    if (searchContext.fullScan || artistImageNeedsUpdate(artist->getImage(), imageInfo))
                        artistImageInfoList.push_back(ArtistImageInfo{ artist->getId(), imageInfo });
                });
            }

            return artistId != searchContext.lastRetrievedArtistId;
        }

        void updateArtistImage(db::Session& session, const ArtistImageInfo& artistImageInfo)
        {
            db::Artist::pointer artist{ db::Artist::find(session, artistImageInfo.artistId) };
            assert(artist);

            db::Image::pointer image{ artist->getImage() };
            const ImageInfo& imageInfo{ artistImageInfo.imageInfo };

            if (!imageInfo)
            {
                if (image)
                    image.remove();
                return;
            }

            if (!image)
            {
                image = session.create<db::Image>(imageInfo.imagePath);
                image.modify()->setArtist(artist);
            }
            else
                image.modify()->setPath(imageInfo.imagePath);

            image.modify()->setLastWriteTime(imageInfo.lastWriteTime);
            image.modify()->setFileSize(imageInfo.fileSize);
            image.modify()->setHeight(imageInfo.height);
            image.modify()->setWidth(imageInfo.width);
        }

        void updateArtistImages(db::Session& session, ArtistImageInfoContainer& imageInfoList)
        {
            if (imageInfoList.empty())
                return;

            auto transaction{ session.createWriteTransaction() };

            for (std::size_t i{}; !imageInfoList.empty() && i < writeBatchSize; ++i)
            {
                updateArtistImage(session, imageInfoList.front());
                imageInfoList.pop_front();
            }
        }

        std::vector<std::string> constructArtistFileNames()
        {
            std::vector<std::string> res;

            core::Service<core::IConfig>::get()->visitStrings("artist-image-file-names",
                [&res](std::string_view fileName) {
                    res.emplace_back(fileName);
                },
                { "artist" });

            return res;
        }

    } // namespace

    ScanStepScanArtistImages::ScanStepScanArtistImages(InitParams& initParams)
        : ScanStepBase{ initParams }
        , _artistFileNames{ constructArtistFileNames() }
    {
    }

    void ScanStepScanArtistImages::process(ScanContext& context)
    {
        auto& session{ _db.getTLSSession() };

        {
            auto transaction{ session.createReadTransaction() };
            context.currentStepStats.totalElems = db::Artist::getCount(session);
        }

        SearchImageContext searchContext{
            .session = session,
            .lastRetrievedArtistId = {},
            .artistFileNames = _artistFileNames,
            .fullScan = context.scanOptions.fullScan
        };

        ArtistImageInfoContainer imageInfoList;
        while (fetchNextArtistImagesToUpdate(searchContext, imageInfoList))
        {
            updateArtistImages(session, imageInfoList);
            context.currentStepStats.processedElems += readBatchSize;
            _progressCallback(context.currentStepStats);
        }
    }
} // namespace lms::scanner
