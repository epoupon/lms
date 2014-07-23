
#include <Wt/WGroupBox>
#include <Wt/WText>
#include <Wt/WPushButton>
#include <Wt/WMessageBox>

#include <Wt/Auth/Identity>

#include "SettingsUsers.hpp"

namespace UserInterface {
namespace Settings {

Users::Users(SessionData& sessionData, Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent),
_sessionData(sessionData)
{
	// Stack two widgets:
	_stack = new Wt::WStackedWidget(this);

	// 1/ the user table view
	{
		Wt::WGroupBox *container = new Wt::WGroupBox("Users", _stack);

		_table = new Wt::WTable(container);

		_table->addStyleClass("table form-inline");

		_table->toggleStyleClass("table-hover", true);
		_table->toggleStyleClass("table-striped", true);

		_table->setHeaderCount(1);

		_table->elementAt(0, 0)->addWidget(new Wt::WText("#"));
		_table->elementAt(0, 1)->addWidget(new Wt::WText("Name"));
		_table->elementAt(0, 2)->addWidget(new Wt::WText("e-Mail"));
		_table->elementAt(0, 3)->addWidget(new Wt::WText("Admin"));

		Wt::WPushButton* addBtn = new Wt::WPushButton("Add User");
		addBtn->setStyleClass("btn-success");
		container->addWidget( addBtn );
		addBtn->clicked().connect(this, &Users::handleAddUser);
	}

	// 2/ the user form
	{
		new Wt::WText("This is the user form!", _stack);
	}

	refresh();
}

void
Users::refresh(void)
{

	assert(_table->rowCount() > 0);
	for (int i = _table->rowCount() - 1; i > 0; ++i)
		_table->deleteRow(i);

	Database::Handler& db = _sessionData.getDatabaseHandler();

	Wt::Dbo::Transaction transaction(db.getSession());

	const Wt::Auth::User& currentUser = db.getLogin().user();

	std::vector<Database::User::pointer> users = Database::User::getAll(db.getSession());

	for (std::size_t i = 0; i < users.size(); ++i)
	{

		std::string userId;
		{
			std::ostringstream oss; oss << users[i].id();
			userId = oss.str();
		}

		Wt::Auth::User authUser = db.getUserDatabase().findWithId( userId );

		_table->elementAt(i + 1, 0)->addWidget(new Wt::WText( Wt::WString::fromUTF8("{1}").arg(i+1)));
		_table->elementAt(i + 1, 1)->addWidget(new Wt::WText( authUser.identity(Wt::Auth::Identity::LoginName)) );

		Wt::WText* email = new Wt::WText();
		if (!authUser.email().empty()) {
			email->setText(authUser.email());
		}
		else {
			email->setStyleClass("alert-danger");
			email->setText(authUser.unverifiedEmail());
		}
		_table->elementAt(i + 1, 2)->addWidget( email ) ;
		_table->elementAt(i + 1, 3)->addWidget(new Wt::WText( users[i]->isAdmin() ? "Yes" : "No" ));

		Wt::WPushButton* editBtn = new Wt::WPushButton("Edit");
		_table->elementAt(i + 1, 4)->addWidget(editBtn);
		editBtn->clicked().connect(boost::bind( &Users::handleEditUser, this, userId));

		if (currentUser != authUser)
		{
			Wt::WPushButton* delBtn = new Wt::WPushButton("Delete");
			delBtn->setStyleClass("btn-danger");
			delBtn->setMargin(5, Wt::Left);
			_table->elementAt(i + 1, 4)->addWidget(delBtn);
			delBtn->clicked().connect(boost::bind( &Users::handleDelUser, this, authUser.identity(Wt::Auth::Identity::LoginName), userId));
		}
	}

}

void
Users::handleDelUser(Wt::WString loginNameIdentity, std::string id)
{
	Wt::WMessageBox *messageBox = new Wt::WMessageBox
		("Delete User",
		Wt::WString( "Deleting user '{1}'?").arg(loginNameIdentity),
		 Wt::Question, Wt::Yes | Wt::No);

	messageBox->setModal(true);

	messageBox->buttonClicked().connect(std::bind([=] () {
		if (messageBox->buttonResult() == Wt::Yes)
		{
			Database::Handler& db = _sessionData.getDatabaseHandler();

			// Delete the user
			Wt::Auth::User authUser = db.getUserDatabase().findWithId( id );

			db.getUserDatabase().deleteUser( authUser );

			// TODO remove Database::User too?

			// TODO Update the view
		}

		delete messageBox;
	}));

	messageBox->show();
}

void
Users::handleAddUser(void)
{
	// TODO refresh the User Form for a new user
	//_stack->setCurrentIndex(1);
}

void
Users::handleEditUser(std::string id)
{
	//TODO refresh the User Form to edit the given user
	//_stack->setCurrentIndex(1);
}


} // namespace UserInterface
} // namespace Settings


