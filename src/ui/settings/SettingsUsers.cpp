/*
 * Copyright (C) 2013 Emeric Poupon
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


#include <Wt/WGroupBox>
#include <Wt/WText>
#include <Wt/WPushButton>
#include <Wt/WMessageBox>

#include <Wt/Auth/Identity>

#include "logger/Logger.hpp"

#include "SettingsUserFormView.hpp"
#include "LmsApplication.hpp"

#include "SettingsUsers.hpp"

namespace UserInterface {
namespace Settings {

Users::Users(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent)
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

	Wt::Dbo::Transaction transaction(DboSession());

	const Wt::Auth::User& currentUser = CurrentAuthUser();

	std::vector<Database::User::pointer> users = Database::User::getAll(DboSession());

	std::size_t userIndex = 1;
	for (std::size_t i = 0; i < users.size(); ++i)
	{

		std::string userId = Database::User::getId(users[i]);

		Wt::Auth::User authUser;

		// Hack try/catch here since it may fail!
		try {
			authUser = DbHandler().getUserDatabase().findWithId( userId );
		}
		catch(Wt::Dbo::Exception& e)
		{
			LMS_LOG(MOD_UI, SEV_ERROR) << "Caught exception when getting userId=" << userId << ": " << e.code();
			continue;
		}

		if (!authUser.isValid()) {
			LMS_LOG(MOD_UI, SEV_ERROR) << "Users::refresh: skipping invalid userId = " << userId;
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
			Wt::Dbo::Transaction transaction(DboSession());

			// Delete the user
			Wt::Auth::User authUser = DbHandler().getUserDatabase().findWithId( id );

			DbHandler().getUserDatabase().deleteUser( authUser );

			Database::User::pointer user = Database::User::getById(DboSession(), id);
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

	UserFormView* userFormView = new UserFormView(id, _stack);
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


