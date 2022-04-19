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
