#pragma once

#include <chrono>
#include <Wt/WContainerWidget.h>
#include <Wt/WString.h>

#include "Notification.hpp"

namespace UserInterface
{
	class NotificationContainer : public Wt::WContainerWidget
	{
		public:
			void add(Notification::Type type, const Wt::WString& category, const Wt::WString& message, std::chrono::milliseconds duration);

		private:
	};
} // namespace UserInterface
