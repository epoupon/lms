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

#include "MediaLibrariesView.hpp"

#include <Wt/WPushButton.h>

#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "services/scanner/IScannerService.hpp"

#include "LmsApplication.hpp"
#include "MediaLibraryModal.hpp"
#include "ModalManager.hpp"

namespace lms::ui
{
    MediaLibrariesView::MediaLibrariesView()
        : Wt::WTemplate{ Wt::WString::tr("Lms.Admin.MediaLibraries.template") }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);

        _libraries = bindNew<Wt::WContainerWidget>("libraries");
        Wt::WPushButton* addBtn{ bindNew<Wt::WPushButton>("add-btn", Wt::WString::tr("Lms.add")) };
        addBtn->clicked().connect(this, [this] {
            auto mediaLibraryModal{ std::make_unique<MediaLibraryModal>(db::MediaLibraryId{}) };
            MediaLibraryModal* mediaLibraryModalPtr{ mediaLibraryModal.get() };

            mediaLibraryModalPtr->saved().connect(this, [this, mediaLibraryModalPtr](db::MediaLibraryId newMediaLibraryId) {
                Wt::WTemplate* entry{ addEntry() };
                updateEntry(newMediaLibraryId, entry);
                // No need to stop the current scan if we add stuff
                LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Admin.MediaLibraries.media-libraries"), Wt::WString::tr("Lms.Admin.MediaLibrary.library-created"));
                LmsApp->getModalManager().dispose(mediaLibraryModalPtr);
            });

            mediaLibraryModalPtr->cancelled().connect(this, [mediaLibraryModalPtr] {
                LmsApp->getModalManager().dispose(mediaLibraryModalPtr);
            });

            LmsApp->getModalManager().show(std::move(mediaLibraryModal));
        });

        wApp->internalPathChanged().connect(this, [this] {
            refreshView();
        });

        refreshView();
    }

    void MediaLibrariesView::refreshView()
    {
        if (!wApp->internalPathMatches("/admin/libraries"))
            return;

        _libraries->clear();

        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        db::MediaLibrary::find(LmsApp->getDbSession(), [&](const db::MediaLibrary::pointer& mediaLibrary) {
            const db::MediaLibraryId mediaLibraryId{ mediaLibrary->getId() };
            Wt::WTemplate* entry{ addEntry() };
            updateEntry(mediaLibraryId, entry);
        });
    }

    void MediaLibrariesView::showDeleteLibraryModal(db::MediaLibraryId mediaLibraryId, Wt::WTemplate* libraryEntry)
    {
        using namespace db;

        auto modal{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Admin.MediaLibraries.template.delete-library")) };
        modal->addFunction("tr", &Wt::WTemplate::Functions::tr);
        Wt::WWidget* modalPtr{ modal.get() };

        auto* delBtn{ modal->bindNew<Wt::WPushButton>("del-btn", Wt::WString::tr("Lms.delete")) };
        delBtn->clicked().connect([=, this] {
            {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                db::MediaLibrary::pointer mediaLibrary{ MediaLibrary::find(LmsApp->getDbSession(), mediaLibraryId) };
                if (mediaLibrary)
                    mediaLibrary.remove();
            }

            // Don't want the scanner to go on with wrong settings
            core::Service<scanner::IScannerService>::get()->requestReload();
            LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Admin.MediaLibraries.media-libraries"), Wt::WString::tr("Lms.Admin.MediaLibrary.library-deleted"));

            _libraries->removeWidget(libraryEntry);

            LmsApp->getModalManager().dispose(modalPtr);
        });

        auto* cancelBtn{ modal->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel")) };
        cancelBtn->clicked().connect([=] {
            LmsApp->getModalManager().dispose(modalPtr);
        });

        LmsApp->getModalManager().show(std::move(modal));
    }

    Wt::WTemplate* MediaLibrariesView::addEntry()
    {
        return _libraries->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Admin.MediaLibraries.template.entry"));
    }

    void MediaLibrariesView::updateEntry(db::MediaLibraryId mediaLibraryId, Wt::WTemplate* entry)
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };
        db::MediaLibrary::pointer mediaLibrary{ db::MediaLibrary::find(LmsApp->getDbSession(), mediaLibraryId) };

        entry->bindString("name", std::string{ mediaLibrary->getName() }, Wt::TextFormat::Plain);
        entry->bindString("path", std::string{ mediaLibrary->getPath() }, Wt::TextFormat::Plain);

        Wt::WPushButton* editBtn{ entry->bindNew<Wt::WPushButton>("edit-btn", Wt::WString::tr("Lms.template.edit-btn"), Wt::TextFormat::XHTML) };
        editBtn->setToolTip(Wt::WString::tr("Lms.edit"));
        editBtn->clicked().connect([this, mediaLibraryId, entry] {
            auto mediaLibraryModal{ std::make_unique<MediaLibraryModal>(mediaLibraryId) };
            MediaLibraryModal* mediaLibraryModalPtr{ mediaLibraryModal.get() };

            mediaLibraryModalPtr->saved().connect(this, [=, this](db::MediaLibraryId newMediaLibraryId) {
                updateEntry(newMediaLibraryId, entry);

                // Don't want the scanner to go on with wrong settings
                core::Service<scanner::IScannerService>::get()->requestReload();
                LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Admin.MediaLibraries.media-libraries"), Wt::WString::tr("Lms.settings-saved"));

                LmsApp->getModalManager().dispose(mediaLibraryModalPtr);
            });

            mediaLibraryModalPtr->cancelled().connect(this, [mediaLibraryModalPtr] {
                LmsApp->getModalManager().dispose(mediaLibraryModalPtr);
            });

            LmsApp->getModalManager().show(std::move(mediaLibraryModal));
        });

        Wt::WPushButton* delBtn{ entry->bindNew<Wt::WPushButton>("del-btn", Wt::WString::tr("Lms.template.trash-btn"), Wt::TextFormat::XHTML) };
        delBtn->setToolTip(Wt::WString::tr("Lms.delete"));
        delBtn->clicked().connect([this, mediaLibraryId, entry] {
            showDeleteLibraryModal(mediaLibraryId, entry);
        });
    }
} // namespace lms::ui