/*
 * Copyright (C) 2026 Emeric Poupon
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

#include "FoldersView.hpp"

#include <algorithm>
#include <unordered_map>

#include <Wt/WAnchor.h>
#include <Wt/WApplication.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>

#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Release.hpp"

#include "LmsApplication.hpp"
#include "ReleaseHelpers.hpp"
#include "common/Template.hpp"
#include "explore/Filters.hpp"
#include "explore/PlayQueueController.hpp"

namespace lms::ui
{
    namespace
    {
        std::string getDirectoryDisplayName(const db::Directory::pointer& directory)
        {
            std::string displayName{ directory->getName() };
            if (displayName.empty())
                displayName = directory->getAbsolutePath().string();
            if (displayName.empty())
                displayName = "/";

            return displayName;
        }
    } // namespace

    Folders::Folders(Filters& filters, PlayQueueController& playQueueController)
        : Template{ Wt::WString::tr("Lms.Explore.Folders.template") }
        , _filters{ filters }
        , _playQueueController{ playQueueController }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);
        addFunction("id", &Wt::WTemplate::Functions::id);

        _breadcrumbs = bindNew<Wt::WContainerWidget>("breadcrumbs");
        _directories = bindNew<Wt::WContainerWidget>("directories");
        _releases = bindNew<Wt::WContainerWidget>("releases");

        bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.Explore.play"), Wt::TextFormat::XHTML)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::Play, _currentReleaseIds);
            });

        bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, _currentReleaseIds);
            });

        bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayNext, _currentReleaseIds);
            });

        bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"), Wt::TextFormat::Plain)
            ->clicked()
            .connect([this] {
                _playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, _currentReleaseIds);
            });

        LmsApp->internalPathChanged().connect(this, [this] {
            onPathOrFilterChanged();
        });
        _filters.updated().connect(this, [this] {
            onPathOrFilterChanged();
        });

        onPathOrFilterChanged();
    }

    bool Folders::isOnFoldersPath() const
    {
        return wApp->internalPathMatches("/folders") || wApp->internalPathMatches("/folder");
    }

    bool Folders::hasContentFilters() const
    {
        const db::Filters& filters{ _filters.getDbFilters() };
        return !filters.clusters.empty() || filters.label.isValid() || filters.releaseType.isValid() || filters.codec.has_value();
    }

    void Folders::onPathOrFilterChanged()
    {
        if (!isOnFoldersPath())
            return;

        refreshView();
    }

    void Folders::refreshView()
    {
        _breadcrumbs->clear();
        _directories->clear();
        _releases->clear();
        _currentReleaseIds.clear();

        setCondition("if-has-subfolders", false);
        setCondition("if-has-releases", false);
        setCondition("if-has-playable", false);
        setCondition("if-empty", false);

        if (const auto directoryId{ getDirectoryFromPath() })
            renderDirectory(*directoryId);
        else
            renderRootDirectories();

        setCondition("if-has-playable", !_currentReleaseIds.empty());
        setCondition("if-empty", !_directories->count() && !_releases->count());
    }

    std::optional<db::DirectoryId> Folders::getDirectoryFromPath() const
    {
        if (!wApp->internalPathMatches("/folder/"))
            return std::nullopt;

        if (const auto rawDirectoryId{ core::stringUtils::readAs<db::DirectoryId::ValueType>(wApp->internalPathNextPart("/folder/")) })
            return db::DirectoryId{ *rawDirectoryId };

        return std::nullopt;
    }

    std::vector<db::Release::pointer> Folders::getReleasesInDirectory(db::DirectoryId directoryId) const
    {
        db::Release::FindParameters params;
        params.setDirectory(directoryId);
        params.setFilters(_filters.getDbFilters());
        params.setSortMethod(db::ReleaseSortMethod::SortName);
        params.setRange(db::Range{ 0, _maxReleaseCount });

        return db::Release::find(LmsApp->getDbSession(), params).results;
    }

    void Folders::renderBreadcrumbs(std::optional<db::DirectoryId> currentDirectoryId)
    {
        Wt::WAnchor* rootAnchor{ _breadcrumbs->addNew<Wt::WAnchor>(Wt::WLink{ Wt::LinkType::InternalPath, "/folders" }, Wt::WString::tr("Lms.Explore.folders")) };
        rootAnchor->addStyleClass("text-decoration-none link-secondary");

        if (!currentDirectoryId)
            return;

        const auto ancestors{ db::Directory::findBreadcrumbs(LmsApp->getDbSession(), *currentDirectoryId) };

        for (std::size_t i{}; i < ancestors.size(); ++i)
        {
            _breadcrumbs->addNew<Wt::WText>(" / ");

            const auto& [directoryId, displayName]{ ancestors[i] };

            if (i + 1 == ancestors.size())
            {
                _breadcrumbs->addNew<Wt::WText>(Wt::WString::fromUTF8(displayName), Wt::TextFormat::Plain);
            }
            else
            {
                Wt::WAnchor* directoryAnchor{ _breadcrumbs->addNew<Wt::WAnchor>(
                    Wt::WLink{ Wt::LinkType::InternalPath, "/folder/" + directoryId.toString() }) };
                directoryAnchor->setTextFormat(Wt::TextFormat::Plain);
                directoryAnchor->setText(Wt::WString::fromUTF8(displayName));
                directoryAnchor->addStyleClass("text-decoration-none link-secondary");
            }
        }
    }

    void Folders::renderDirectories(const std::vector<db::Directory::pointer>& directories, const std::unordered_map<db::DirectoryId::ValueType, db::ReleaseId>& directReleaseTargets)
    {
        if (directories.empty())
            return;

        setCondition("if-has-subfolders", true);

        for (const db::Directory::pointer& directory : directories)
        {
            Template* entry{ _directories->addNew<Template>(Wt::WString::tr("Lms.Explore.Folders.template.folder-entry")) };
            const std::string displayName{ getDirectoryDisplayName(directory) };
            std::string targetPath{ "/folder/" + directory->getId().toString() };

            if (const auto it{ directReleaseTargets.find(directory->getId().getValue()) }; it != std::cend(directReleaseTargets))
                targetPath = "/release/" + it->second.toString();

            auto directoryAnchor{ std::make_unique<Wt::WAnchor>(
                Wt::WLink{ Wt::LinkType::InternalPath, targetPath }) };
            directoryAnchor->setTextFormat(Wt::TextFormat::Plain);
            directoryAnchor->setText(Wt::WString::fromUTF8(displayName));

            entry->bindWidget("directory", std::move(directoryAnchor));
        }
    }

    void Folders::renderReleases(const std::vector<db::Release::pointer>& releases)
    {
        if (releases.empty())
            return;

        setCondition("if-has-releases", true);

        _currentReleaseIds.reserve(releases.size());
        for (const db::Release::pointer& release : releases)
        {
            _currentReleaseIds.push_back(release->getId());
            _releases->addWidget(releaseListHelpers::createEntry(release, { releaseListHelpers::DisplayOptions::ShowArtist }));
        }
    }

    void Folders::renderRootDirectories()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        std::vector<db::Directory::pointer> directories;
        std::unordered_map<db::DirectoryId::ValueType, db::ReleaseId> directReleaseTargets;

        const db::MediaLibraryId mediaLibraryId{ _filters.getDbFilters().mediaLibrary };
        const auto processResults{ [&](const auto& results) {
            for (const auto& [dir, releaseCount, singleReleaseId] : results)
            {
                directories.push_back(dir);
                if (singleReleaseId.isValid())
                    directReleaseTargets.emplace(dir->getId().getValue(), singleReleaseId);
            }
        } };

        if (hasContentFilters())
            processResults(db::Directory::findFilteredFolderListing(LmsApp->getDbSession(), std::nullopt, _filters.getDbFilters()));
        else
            processResults(db::Directory::findFolderListing(LmsApp->getDbSession(), std::nullopt, mediaLibraryId.isValid() ? std::optional{ mediaLibraryId } : std::nullopt));

        renderBreadcrumbs({});
        renderDirectories(directories, directReleaseTargets);
    }

    void Folders::renderDirectory(db::DirectoryId directoryId)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::Directory::pointer directory{ db::Directory::find(LmsApp->getDbSession(), directoryId) };
        if (!directory)
            return;

        if (const db::MediaLibraryId mediaLibraryFilter{ _filters.getDbFilters().mediaLibrary }; mediaLibraryFilter.isValid())
        {
            const db::MediaLibrary::pointer mediaLibrary{ directory->getMediaLibrary() };
            if (!mediaLibrary || mediaLibrary->getId() != mediaLibraryFilter)
                return;
        }

        std::vector<db::Directory::pointer> subDirectories;
        std::unordered_map<db::DirectoryId::ValueType, db::ReleaseId> directReleaseTargets;

        const db::MediaLibraryId mediaLibraryId{ _filters.getDbFilters().mediaLibrary };
        const auto processResults{ [&](const auto& results) {
            for (const auto& [dir, releaseCount, singleReleaseId] : results)
            {
                subDirectories.push_back(dir);
                if (singleReleaseId.isValid())
                    directReleaseTargets.emplace(dir->getId().getValue(), singleReleaseId);
            }
        } };

        if (hasContentFilters())
            processResults(db::Directory::findFilteredFolderListing(LmsApp->getDbSession(), directory->getId(), _filters.getDbFilters()));
        else
            processResults(db::Directory::findFolderListing(LmsApp->getDbSession(), directory->getId(), mediaLibraryId.isValid() ? std::optional{ mediaLibraryId } : std::nullopt));

        const auto releases{ getReleasesInDirectory(directory->getId()) };

        renderBreadcrumbs(directory->getId());
        renderDirectories(subDirectories, directReleaseTargets);
        renderReleases(releases);
    }
} // namespace lms::ui
