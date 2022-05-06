/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "NotificationContainer.hpp"

#include <sstream>
#include <Wt/WTemplate.h>

#include "utils/Logger.hpp"
#include "LmsApplication.hpp"

namespace UserInterface
{
	class NotificationWidget : public Wt::WTemplate
	{
		public:
			NotificationWidget(Notification::Type type, const Wt::WString& category, const Wt::WString& message, std::chrono::milliseconds duration);
			Wt::JSignal<> closed {this, "closed"};
	};

	NotificationWidget::NotificationWidget(Notification::Type type, const Wt::WString& category, const Wt::WString& message, std::chrono::milliseconds duration)
		: Wt::WTemplate {Wt::WString::tr("Lms.notifications.template.entry")}
	{
		switch (type)
		{
			case Notification::Type::Info:
				bindString("bg-color", "bg-primary");
				bindString("text-color", "white");
				break;
			case Notification::Type::Warning:
				bindString("bg-color", "bg-warning");
				bindString("text-color", "dark");
				break;
			case Notification::Type::Danger:
				bindString("bg-color", "bg-danger");
				bindString("text-color", "white");
				break;
		}

		bindString("category", category);
		bindString("message", message);
		bindInt("duration", duration.count());

		std::ostringstream oss;

		oss
			<< R"({const toastElement = )" << jsRef() << R"(.getElementsByClassName('toast')[0];)"
			<< R"(const toast = bootstrap.Toast.getOrCreateInstance(toastElement);)"
			<< R"(toast.show();)"
			<< R"(toastElement.addEventListener('hidden.bs.toast', function () {)"
				<< closed.createCall({})
				<< R"(toast.dispose();)"
			<< R"(});})";

		LMS_LOG(UI, DEBUG) << "Running JS '" << oss.str() << "'";

		doJavaScript(oss.str());
	}

	void
	NotificationContainer::add(Notification::Type type, const Wt::WString& category, const Wt::WString& message, std::chrono::milliseconds duration)
	{
		NotificationWidget* notification {addNew<NotificationWidget>(type, category, message, duration)};

		notification->closed.connect([=]
		{
			removeWidget(notification);
		});
	}
}
