#include <Wt/WMenu>
#include <Wt/WStackedWidget>
#include <Wt/WTextArea>

#include "SettingsDatabaseFormView.hpp"

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

	if (user->isAdmin())
	{
		// TODO Special admin settings
		menu->addItem("Database", new DatabaseFormView(sessionData));
		menu->addItem("Users", new Wt::WTextArea("Users here!"));
	}

	// User specifics settings
	menu->addItem("Transcoding", new Wt::WTextArea("User's transcoding settings here!"));
	menu->addItem("Personal", new Wt::WTextArea("User's password + mail here!"));

	addWidget(contents);
}

} // namespace Settings
} // namespace UserInterface
