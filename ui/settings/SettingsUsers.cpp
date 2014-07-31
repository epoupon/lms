
#include <Wt/WGroupBox>
#include <Wt/WText>
#include <Wt/WPushButton>
#include <Wt/WMessageBox>

#include <Wt/Auth/Identity>

#include "SettingsUserFormView.hpp"

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
		addBtn->clicked().connect(boost::bind(&Users::handleCreateUser, this, ""));
	}

	refresh();
}

void
Users::refresh(void)
{

	assert(_table->rowCount() > 0);
	for (int i = _table->rowCount() - 1; i > 0; --i)
		_table->deleteRow(i);

	Database::Handler& db = _sessionData.getDatabaseHandler();

	Wt::Dbo::Transaction transaction(db.getSession());

	const Wt::Auth::User& currentUser = db.getLogin().user();

	std::vector<Database::User::pointer> users = Database::User::getAll(db.getSession());

	std::size_t userIndex = 1;
	for (std::size_t i = 0; i < users.size(); ++i)
	{

		std::string userId = Database::User::getId(users[i]);

		Wt::Auth::User authUser;

		// Hack try/catch here since it may fail!
		try {
			authUser = db.getUserDatabase().findWithId( userId );
		}
		catch(Wt::Dbo::Exception& e)
		{
			std::cerr << "Caught exception when getting userId=" << userId << ": " << e.code() << std::endl;
			continue;
		}

		if (!authUser.isValid()) {
			std::cerr << "Users::refresh: skipping invalid userId = " << userId << std::endl;
			continue;
		}

		_table->elementAt(userIndex, 0)->addWidget(new Wt::WText( Wt::WString::fromUTF8("{1}").arg(userIndex)));
		_table->elementAt(userIndex, 1)->addWidget(new Wt::WText( authUser.identity(Wt::Auth::Identity::LoginName)) );

		Wt::WText* email = new Wt::WText();
		if (!authUser.email().empty()) {
			email->setText(authUser.email());
		}
		else {
			email->setStyleClass("alert-danger");
			email->setText(authUser.unverifiedEmail());
		}
		_table->elementAt(userIndex, 2)->addWidget( email ) ;
		_table->elementAt(userIndex, 3)->addWidget(new Wt::WText( users[i]->isAdmin() ? "Yes" : "No" ));

		Wt::WPushButton* editBtn = new Wt::WPushButton("Edit");
		_table->elementAt(userIndex, 4)->addWidget(editBtn);
		editBtn->clicked().connect(boost::bind( &Users::handleCreateUser, this, userId));

		if (currentUser != authUser)
		{
			Wt::WPushButton* delBtn = new Wt::WPushButton("Delete");
			delBtn->setStyleClass("btn-danger");
			delBtn->setMargin(5, Wt::Left);
			_table->elementAt(userIndex, 4)->addWidget(delBtn);
			delBtn->clicked().connect(boost::bind( &Users::handleDelUser, this, authUser.identity(Wt::Auth::Identity::LoginName), userId));
		}

		++userIndex;
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

			Wt::Dbo::Transaction transaction(db.getSession());

			// Delete the user
			Wt::Auth::User authUser = db.getUserDatabase().findWithId( id );

			db.getUserDatabase().deleteUser( authUser );

			Database::User::pointer user = Database::User::getById(db.getSession(), id);
			if (user)
				user.remove();

			refresh();
		}

		delete messageBox;
	}));

	messageBox->show();

}

void
Users::handleCreateUser(std::string id)
{
	assert(_stack->count() == 1);

	UserFormView* userFormView = new UserFormView(_sessionData, id, _stack);
	userFormView->completed().connect(this, &Users::handleUserFormCompleted);

	_stack->setCurrentIndex(1);
}

void
Users::handleUserFormCompleted(bool changed)
{
	_stack->setCurrentIndex(0);

	// Refresh the user table if a change has been made
	if (changed)
		refresh();

	// Delete the form view
	delete _stack->widget(1);

}

} // namespace UserInterface
} // namespace Settings


