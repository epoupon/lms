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

#include "MediaLibraryModal.hpp"

#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplateFormView.h>

#include "core/Path.hpp"
#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/MediaLibrary.hpp"

#include "LmsApplication.hpp"

namespace lms::ui
{
    using namespace db;

    namespace
    {
        class LibraryNameValidator : public Wt::WValidator
        {
        public:
            LibraryNameValidator(MediaLibraryId libraryId)
                : _libraryId{ libraryId } {}

        private:
            Wt::WValidator::Result validate(const Wt::WString& input) const override
            {
                if (input.empty())
                    return Wt::WValidator::validate(input);

                Wt::WValidator::Result result{ Wt::ValidationState::Valid };
                const std::string name{ input.toUTF8() };

                auto& session{ LmsApp->getDbSession() };
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };

                db::MediaLibrary::find(session, [&](const db::MediaLibrary::pointer library) {
                    if (library->getId() == _libraryId)
                        return;

                    if (core::stringUtils::stringCaseInsensitiveEqual(name, library->getName()))
                        result = Wt::WValidator::Result{ Wt::ValidationState::Invalid, Wt::WString::tr("Lms.Admin.MediaLibrary.name-already-exists") };
                });

                return result;
            }

            const MediaLibraryId _libraryId;
        };

        class LibraryRootPathValidator : public Wt::WValidator
        {
        public:
            LibraryRootPathValidator(MediaLibraryId libraryId)
                : _libraryId{ libraryId } {}

        private:
            Wt::WValidator::Result validate(const Wt::WString& input) const override
            {
                if (input.empty())
                    return Wt::WValidator::validate(input);

                const std::filesystem::path p{ input.toUTF8() };
                std::error_code ec;

                if (p.is_relative())
                    return Wt::WValidator::Result(Wt::ValidationState::Invalid, Wt::WString::tr("Lms.Admin.MediaLibrary.path-must-be-absolute"));

                // TODO check and translate access rights issues
                bool res{ std::filesystem::is_directory(p, ec) };
                if (ec)
                    return Wt::WValidator::Result(Wt::ValidationState::Invalid, ec.message()); // TODO translate common errors
                else if (!res)
                    return Wt::WValidator::Result(Wt::ValidationState::Invalid, Wt::WString::tr("Lms.Admin.MediaLibrary.path-must-be-existing-directory"));

                auto& session{ LmsApp->getDbSession() };
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };

                Wt::WValidator::Result result{ Wt::ValidationState::Valid };
                const std::filesystem::path rootPath{ std::filesystem::path{ input.toUTF8() }.lexically_normal() };
                db::MediaLibrary::find(session, [&](const db::MediaLibrary::pointer library) {
                    if (library->getId() == _libraryId)
                        return;

                    const std::filesystem::path libraryRootPath{ library->getPath().lexically_normal() };

                    if (core::pathUtils::isPathInRootPath(rootPath, libraryRootPath)
                        || core::pathUtils::isPathInRootPath(libraryRootPath, rootPath))
                    {
                        result = Wt::WValidator::Result{ Wt::ValidationState::Invalid, Wt::WString::tr("Lms.Admin.MediaLibrary.path-must-not-overlap") };
                    }
                });

                return result;
            }

            const MediaLibraryId _libraryId;
        };

        class MediaLibraryModel : public Wt::WFormModel
        {
        public:
            static inline constexpr Field NameField{ "name" };
            static inline constexpr Field DirectoryField{ "directory" };

            MediaLibraryModel(MediaLibraryId libraryId)
                : _libraryId{ libraryId }
            {
                addField(NameField);
                addField(DirectoryField);

                {
                    auto nameValidator{ std::make_shared<LibraryNameValidator>(libraryId) };
                    nameValidator->setMandatory(true);
                    setValidator(NameField, std::move(nameValidator));
                }

                {
                    auto directoryValidator{ std::make_shared<LibraryRootPathValidator>(libraryId) };
                    directoryValidator->setMandatory(true);
                    setValidator(DirectoryField, std::move(directoryValidator));
                }

                if (libraryId.isValid())
                    loadData();
            }

            MediaLibraryId saveData()
            {
                auto& session{ LmsApp->getDbSession() };
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                std::string name{ valueText(NameField).toUTF8() };
                std::string path{ valueText(DirectoryField).toUTF8() };

                MediaLibrary::pointer library;
                if (_libraryId.isValid())
                {
                    library = MediaLibrary::find(session, _libraryId);
                    if (library)
                    {
                        library.modify()->setName(name);
                        library.modify()->setPath(path);
                    }
                }
                else
                {
                    library = session.create<MediaLibrary>(name, path);
                }

                return library->getId();
            }

        private:
            void loadData()
            {
                auto& session{ LmsApp->getDbSession() };
                auto transaction{ session.createReadTransaction() };

                const MediaLibrary::pointer library{ MediaLibrary::find(session, _libraryId) };

                setValue(NameField, std::string{ library->getName() });
                setValue(DirectoryField, library->getPath().string());
            }

            const MediaLibraryId _libraryId;
        };
    } // namespace

    MediaLibraryModal::MediaLibraryModal(MediaLibraryId mediaLibraryId)
        : Wt::WTemplateFormView{ Wt::WString::tr("Lms.Admin.MediaLibrary.template") }
    {
        auto model{ std::make_shared<MediaLibraryModel>(mediaLibraryId) };

        bindString("title", Wt::WString::tr(mediaLibraryId.isValid() ? "Lms.Admin.MediaLibrary.edit-library" : "Lms.Admin.MediaLibrary.create-library"));

        setFormWidget(MediaLibraryModel::NameField, std::make_unique<Wt::WLineEdit>());
        setFormWidget(MediaLibraryModel::DirectoryField, std::make_unique<Wt::WLineEdit>());

        Wt::WPushButton* saveBtn{ bindNew<Wt::WPushButton>("save-btn", Wt::WString::tr(mediaLibraryId.isValid() ? "Lms.save" : "Lms.create")) };
        saveBtn->clicked().connect(this, [this, model] {
            updateModel(model.get());

            if (model->validate())
            {
                db::MediaLibraryId mediaLibraryId{ model->saveData() };
                saved().emit(mediaLibraryId);
            }
            else
            {
                updateView(model.get());
            }
        });

        Wt::WPushButton* cancelBtn{ bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel")) };
        cancelBtn->clicked().connect(this, [this] { cancelled().emit(); });

        updateView(model.get());
    }
} // namespace lms::ui
