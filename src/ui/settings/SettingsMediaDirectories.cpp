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

#include "database/MediaDirectory.hpp"

#include "LmsApplication.hpp"

#include "SettingsMediaDirectoryFormView.hpp"

#include "SettingsMediaDirectories.hpp"

namespace UserInterface {
namespace Settings {

MediaDirectories::MediaDirectories(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent)
{
	// Stack two widgets:
	_stack = new Wt::WStackedWidget(this);

	// 1/ the media directory table view
	{
		Wt::WGroupBox *container = new Wt::WGroupBox("Media Folders", _stack);

		_table = new Wt::WTable(container);

		_table->addStyleClass("table form-inline");

		_table->toggleStyleClass("table-hover", true);
		_table->toggleStyleClass("table-striped", true);

		_table->setHeaderCount(1);

		_table->elementAt(0, 0)->addWidget(new Wt::WText("#"));
		_table->elementAt(0, 1)->addWidget(new Wt::WText("Path"));
		_table->elementAt(0, 2)->addWidget(new Wt::WText("Type"));

		Wt::WPushButton* addBtn = new Wt::WPushButton("Add Folder");
		addBtn->setStyleClass("btn-success");
		container->addWidget( addBtn );
		addBtn->clicked().connect(boost::bind(&MediaDirectories::handleCreateMediaDirectory, this));
	}

	refresh();
}

void
MediaDirectories::refresh(void)
{

	assert(_table->rowCount() > 0);
	for (int i = _table->rowCount() - 1; i > 0; --i)
		_table->deleteRow(i);

	Wt::Dbo::Transaction transaction(DboSession());

	std::vector<Database::MediaDirectory::pointer> mediaDirectories = Database::MediaDirectory::getAll(DboSession());

	std::size_t id = 1;
	BOOST_FOREACH(Database::MediaDirectory::pointer mediaDirectory, mediaDirectories)
	{
		_table->elementAt(id, 0)->addWidget(new Wt::WText( Wt::WString::fromUTF8("{1}").arg(id)));
		_table->elementAt(id, 1)->addWidget(new Wt::WText( Wt::WString::fromUTF8(mediaDirectory->getPath().string()) ));

		Wt::WString dirType;
		switch(mediaDirectory->getType())
		{
			case Database::MediaDirectory::Video:	dirType = "Video"; break;
			case Database::MediaDirectory::Audio:	dirType = "Audio"; break;
		}

		_table->elementAt(id, 2)->addWidget( new Wt::WText( dirType ));

		Wt::WPushButton* delBtn = new Wt::WPushButton("Delete");
		delBtn->setStyleClass("btn-danger");
		_table->elementAt(id, 3)->addWidget(delBtn);
		delBtn->clicked().connect(boost::bind( &MediaDirectories::handleDelMediaDirectory, this, mediaDirectory->getPath(), mediaDirectory->getType() ) );

		++id;
	}
}

void
MediaDirectories::handleDelMediaDirectory(boost::filesystem::path p, Database::MediaDirectory::Type type)
{
	Wt::WMessageBox *messageBox = new Wt::WMessageBox
		("Delete Folder",
		Wt::WString( "Deleting folder '{1}'?").arg(p.string()),
		 Wt::Question, Wt::Yes | Wt::No);

	messageBox->setModal(true);

	messageBox->buttonClicked().connect(std::bind([=] () {
		if (messageBox->buttonResult() == Wt::Yes)
		{
			{
				Wt::Dbo::Transaction transaction(DboSession());

				// Delete the media diretory
				Database::MediaDirectory::pointer mediaDirectory = Database::MediaDirectory::get(DboSession(), p, type);
				if (mediaDirectory)
					mediaDirectory.remove();
				}

				refresh();

				// Emit something changed in the settings
				_sigChanged.emit();
			}

		delete messageBox;
	}));

	messageBox->show();

}

void
MediaDirectories::handleCreateMediaDirectory(void)
{
	assert(_stack->count() == 1);

	MediaDirectoryFormView* formView = new MediaDirectoryFormView(_stack);
	formView->completed().connect(this, &MediaDirectories::handleMediaDirectoryFormCompleted);

	_stack->setCurrentIndex(1);
}

void
MediaDirectories::handleMediaDirectoryFormCompleted(bool changed)
{
	_stack->setCurrentIndex(0);

	if (changed)
	{
		// Refresh the user table if a change has been made
		refresh();

		// Emit something changed in the settings
		_sigChanged.emit();
	}

	// Delete the form view
	delete _stack->widget(1);

}

} // namespace UserInterface
} // namespace Settings


