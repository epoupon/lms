#include <Wt/WMenu>
#include <Wt/WStackedWidget>
#include <Wt/WTextArea>

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
	// Create a stack where the contents will be located.
	Wt::WStackedWidget *contents = new Wt::WStackedWidget();

	Wt::WMenu *menu = new Wt::WMenu(contents, Wt::Vertical, this);
	menu->setStyleClass("nav nav-pills nav-stacked");
	menu->setWidth(150);

	Wt::Dbo::Transaction transaction( sessionData.getDatabaseHandler().getSession());

	::Database::User::pointer user = sessionData.getDatabaseHandler().getCurrentUser();

	// Must be logged in here
	assert(user);

	menu->addItem("Audio", new AudioFormView(sessionData, Database::User::getId(user)));
	if (user->isAdmin())
	{
		MediaDirectories* mediaDirectory = new MediaDirectories(sessionData);
		mediaDirectory->changed().connect(this, &Settings::handleDatabaseSettingsChanged);
		menu->addItem("Media Folders", mediaDirectory);

		DatabaseFormView* databaseFormView = new DatabaseFormView(sessionData);
		databaseFormView->changed().connect(this, &Settings::handleDatabaseSettingsChanged);
		menu->addItem("Database Update", databaseFormView);

		menu->addItem("Users", new Users(sessionData));
	}
	else
	{
		menu->addItem("Account", new AccountFormView(sessionData, Database::User::getId(user)));
	}

	addWidget(contents);
}

void
Settings::handleDatabaseSettingsChanged()
{
	// On settings change, request an immediate scan
	{
		Wt::Dbo::Transaction transaction(_sessionData.getDatabaseHandler().getSession());
		Database::MediaDirectorySettings::get(_sessionData.getDatabaseHandler().getSession()).modify()->setManualScanRequested(true);
	}

	// Restarting the update service
	boost::lock_guard<boost::mutex> serviceLock (ServiceManager::instance().mutex());

	DatabaseUpdateService::pointer service = ServiceManager::instance().getService<DatabaseUpdateService>();
	if (service)
		service->restart();
}

} // namespace Settings
} // namespace UserInterface
