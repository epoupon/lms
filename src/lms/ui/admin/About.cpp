/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "ModalManager.hpp"

#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>

#include "core/Version.hpp"

#include "LmsApplication.hpp"

namespace lms::ui
{
    void showAboutModal()
    {
        auto aboutModal{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Admin.About.template")) };
        Wt::WWidget* aboutModalPtr{ aboutModal.get() };
        aboutModal->addFunction("tr", &Wt::WTemplate::Functions::tr);

        aboutModal->bindString("version", std::string{ core::getVersion() }, Wt::TextFormat::Plain);
        aboutModal->bindString("homepage-link", "https://github.com/epoupon/lms");

        Wt::WPushButton* okBtn{ aboutModal->bindNew<Wt::WPushButton>("ok-btn", Wt::WString::tr("Lms.ok")) };
        okBtn->clicked().connect([=] {
            LmsApp->getModalManager().dispose(aboutModalPtr);
        });

        LmsApp->getModalManager().show(std::move(aboutModal));
    }
} // namespace lms::ui
