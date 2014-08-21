#include <Wt/WMenu>
#include <Wt/WStackedWidget>
#include <Wt/WTextArea>
#include <Wt/WHBoxLayout>

#include "SettingsAudioFormView.hpp"
#include "SettingsUserFormView.hpp"
#include "SettingsAccountFormView.hpp"
#include "SettingsDatabaseFormView.hpp"
#include "SettingsMediaDirectories.hpp"
#include "SettingsUsers.hpp"

#include "service/ServiceManager.hpp"
#include "service/DatabaseUpdateService.hpp"

#include "Settings.hpp"

namespace UserInterface {
namespace Settings {

Settings::Settings(SessionData& sessionData, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
_sessionData(sessionData)
{
	Wt::WHBoxLayout* hLayout = new Wt::WHBoxLayout(this);

	// Create a stack where the contents will be located.
	Wt::WStackedWidget *contents = new Wt::WStackedWidget();

	contents->setStyleClass("contents");
	contents->setOverflow(WContainerWidget::OverflowHidden);

	Wt::WMenu *menu = new Wt::WMenu(contents, Wt::Vertical);
	menu->setStyleClass("nav nav-pills nav-stacked submenu");
	menu->setWidth(150);

	hLayout->addWidget(menu);
	hLayout->addWidget(contents, 1);

	Wt::Dbo::Transaction transaction( sessionData.getDatabaseHandler().getSession());

	::Database::User::pointer user = sessionData.getDatabaseHandler().getCurrentUser();

	// Must be logged in here
	assert(user);

	menu->addItem("Audio", new AudioFormView(sessionData, Database::User::getId(user)));
	if (user->isAdmin())
	{
		MediaDirectories* mediaDirectory = new MediaDirectories(sessionData);
		mediaDirectory->changed().connect(this, &Settings::handleDatabaseDirectoriesChanged);
		menu->addItem("Media Folders", mediaDirectory);

		DatabaseFormView* databaseFormView = new DatabaseFormView(sessionData);
		databaseFormView->changed().connect(this, &Settings::restartDatabaseUpdateService);
		menu->addItem("Database Update", databaseFormView);

		menu->addItem("Users", new Users(sessionData));
	}
	else
	{
		menu->addItem("Account", new AccountFormView(sessionData, Database::User::getId(user)));
	}

}

void
Settings::handleDatabaseDirectoriesChanged()
{
	std::cout << "Media directories have changed: requesting imediate scan" << std::endl;
	// On directory add or delete, request an immediate scan
	{
		Wt::Dbo::Transaction transaction(_sessionData.getDatabaseHandler().getSession());
		Database::MediaDirectorySettings::get(_sessionData.getDatabaseHandler().getSession()).modify()->setManualScanRequested(true);
	}

	restartDatabaseUpdateService();
}

void
Settings::restartDatabaseUpdateService()
{
	// Restarting the update service
	boost::lock_guard<boost::mutex> serviceLock (Service::ServiceManager::instance().mutex());

	Service::DatabaseUpdateService::pointer service = Service::ServiceManager::instance().getService<Service::DatabaseUpdateService>();
	if (service)
		service->restart();
}

} // namespace Settings
} // namespace UserInterface
