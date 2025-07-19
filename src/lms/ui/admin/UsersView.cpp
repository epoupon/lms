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

#include "UsersView.hpp"

#include <Wt/WMessageBox.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>

#include "core/ILogger.hpp"
#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/objects/User.hpp"
#include "services/auth/IPasswordService.hpp"

#include "LmsApplication.hpp"
#include "ModalManager.hpp"

namespace lms::ui
{
    using namespace db;

    UsersView::UsersView()
        : Wt::WTemplate{ Wt::WString::tr("Lms.Admin.Users.template") }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);

        _container = bindNew<Wt::WContainerWidget>("users");

        if (LmsApp->getAuthBackend() == AuthenticationBackend::Internal)
        {
            setCondition("if-can-create-user", true);

            Wt::WPushButton* addBtn = bindNew<Wt::WPushButton>("add-btn", Wt::WString::tr("Lms.Admin.Users.add"));
            addBtn->clicked().connect([] {
                LmsApp->setInternalPath("/admin/user", true);
            });
        }

        wApp->internalPathChanged().connect(this, [this]() {
            refreshView();
        });

        refreshView();
    }

    void UsersView::refreshView()
    {
        if (!wApp->internalPathMatches("/admin/users"))
            return;

        _container->clear();

        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const UserId currentUserId{ LmsApp->getUserId() };
        User::find(LmsApp->getDbSession(), User::FindParameters{}, [&](const User::pointer& user) {
            const UserId userId{ user->getId() };

            Wt::WTemplate* entry{ _container->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Admin.Users.template.entry")) };

            entry->bindString("name", user->getLoginName(), Wt::TextFormat::Plain);

            // Create tag
            if (user->isAdmin() || user->isDemo())
            {
                entry->setCondition("if-tag", true);
                entry->bindString("tag", Wt::WString::tr(user->isAdmin() ? "Lms.Admin.Users.admin" : "Lms.Admin.Users.demo"));
            }

            // Don't edit ourself this way
            if (user->getId() == currentUserId)
                return;

            entry->setCondition("if-edit", true);
            Wt::WPushButton* editBtn = entry->bindNew<Wt::WPushButton>("edit-btn", Wt::WString::tr("Lms.template.edit-btn"), Wt::TextFormat::XHTML);
            editBtn->setToolTip(Wt::WString::tr("Lms.edit"));
            editBtn->clicked().connect([userId]() {
                LmsApp->setInternalPath("/admin/user/" + userId.toString(), true);
            });

            Wt::WPushButton* delBtn = entry->bindNew<Wt::WPushButton>("del-btn", Wt::WString::tr("Lms.template.trash-btn"), Wt::TextFormat::XHTML);
            delBtn->setToolTip(Wt::WString::tr("Lms.delete"));
            delBtn->clicked().connect([this, userId, entry] {
                auto modal{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Admin.Users.template.delete-user")) };
                modal->addFunction("tr", &Wt::WTemplate::Functions::tr);
                Wt::WWidget* modalPtr{ modal.get() };

                auto* delBtn{ modal->bindNew<Wt::WPushButton>("del-btn", Wt::WString::tr("Lms.delete")) };
                delBtn->clicked().connect([=, this] {
                    {
                        auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                        User::pointer user{ User::find(LmsApp->getDbSession(), userId) };
                        if (user)
                            user.remove();
                    }

                    _container->removeWidget(entry);

                    LmsApp->getModalManager().dispose(modalPtr);
                });

                auto* cancelBtn{ modal->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel")) };
                cancelBtn->clicked().connect([=] {
                    LmsApp->getModalManager().dispose(modalPtr);
                });

                LmsApp->getModalManager().show(std::move(modal));
            });
        });
    }
} // namespace lms::ui
