#include <Wt/WMenu>
#include <Wt/WStackedWidget>
#include <Wt/WTextArea>

#include "SettingsAudioFormView.hpp"
#include "SettingsUserFormView.hpp"
#include "SettingsAccountFormView.hpp"
#include "SettingsDatabaseFormView.hpp"
#include "SettingsUsers.hpp"

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
		menu->addItem("Database", new DatabaseFormView(sessionData));
		menu->addItem("Users", new Users(sessionData));
	}
	else
	{
		menu->addItem("Account", new AccountFormView(sessionData, Database::User::getId(user)));
	}

	addWidget(contents);
}

} // namespace Settings
} // namespace UserInterface
