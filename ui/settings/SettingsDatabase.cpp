#include <map>

#include <Wt/WTable>
#include <Wt/WGroupBox>
#include <Wt/WText>
#include <Wt/WLineEdit>
#include <Wt/WString>
#include <Wt/WPushButton>
#include <Wt/WCheckBox>
#include <Wt/WBreak>
#include <Wt/WMessageBox>

#include "database/MediaDirectory.hpp"
#include "service/ServiceManager.hpp"
#include "service/DatabaseUpdateService.hpp"

#include "common/DirectoryValidator.hpp"

#include "SettingsDatabase.hpp"

namespace UserInterface {
namespace Settings {


Database::Database(SessionData& sessionData, Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent),
_sessionData(sessionData)
{

	// Media directories
	{
		Wt::WGroupBox *container = new Wt::WGroupBox("Media directories", this);

		_table = new Wt::WTable(container);

		_table->addStyleClass("table form-inline");
		_table->toggleStyleClass("table-hover", true);
		_table->toggleStyleClass("table-striped", true);

		_table->setHeaderCount(1);
		_table->elementAt(0, 0)->addWidget(new Wt::WText("Path"));
		_table->elementAt(0, 1)->addWidget(new Wt::WText("Status"));
		_table->elementAt(0, 2)->addWidget(new Wt::WText("Delete"));

		{
			Wt::WPushButton* btn = new Wt::WPushButton("Add");
			container->addWidget(btn);
			btn->clicked().connect(this, &Database::handleAddDirectory);
		}
		{
			Wt::WPushButton* btn = new Wt::WPushButton("Delete");
			container->addWidget(btn);
			btn->clicked().connect(this, &Database::handleDelDirectory);
		}

	}
	addWidget(new Wt::WBreak());

	// Refresh settings
	{
		Wt::WGroupBox *container = new Wt::WGroupBox("Refresh settings", this);

		Wt::WTable *table = new Wt::WTable(container);
		table->addStyleClass("table form-inline");

		table->setHeaderCount(1, Wt::Vertical);
		std::size_t line = 0;
		table->elementAt(line, 0)->addWidget(new Wt::WText("Update period"));
		table->elementAt(line, 1)->addWidget(_updatePeriod = new Wt::WComboBox() );
		// Populate combo
		_updatePeriodModel = new Wt::WStringListModel(_updatePeriod);

		_updatePeriodModel->addString("Never");
		_updatePeriodModel->setData(0, 0, boost::posix_time::time_duration( boost::posix_time::hours(0) ), Wt::UserRole);

		_updatePeriodModel->addString("Daily");
		_updatePeriodModel->setData(1, 0, boost::posix_time::time_duration( boost::posix_time::hours(24) ), Wt::UserRole);

		_updatePeriodModel->addString("Weekly");
		_updatePeriodModel->setData(2, 0, boost::posix_time::time_duration( boost::posix_time::hours(24*7) ), Wt::UserRole);

		_updatePeriodModel->addString("Monthly");
		_updatePeriodModel->setData(3, 0, boost::posix_time::time_duration(boost::posix_time::hours(24*30) ), Wt::UserRole);

		_updatePeriod->setModel(_updatePeriodModel);

		line++;

		table->elementAt(line, 0)->addWidget(new Wt::WText("Update start time"));
		table->elementAt(line, 1)->addWidget(_updateStartTime = new Wt::WComboBox( ));
		// populate combo
		_updateStartTimeModel = new Wt::WStringListModel(_updateStartTime);

		for (std::size_t i = 0; i < 24; ++i)
		{
			boost::posix_time::time_duration dur = boost::posix_time::hours(i);

			boost::posix_time::time_facet* facet = new boost::posix_time::time_facet("%H:%M");
			facet->time_duration_format("%H:%M");

			std::ostringstream oss;
			oss.imbue(std::locale(oss.getloc(), facet));

			oss << dur;

			_updateStartTimeModel->addString( oss.str() );
			_updateStartTimeModel->setData(i, 0, dur, Wt::UserRole);
		}

		_updateStartTime->setModel(_updateStartTimeModel);

		line++;

		Wt::WPushButton* btn = new Wt::WPushButton("Scan now", container);
		btn->clicked().connect(this, &Database::handleScanNow);
	}

	addWidget(new Wt::WBreak());
	addWidget(new Wt::WBreak());
	{
		Wt::WPushButton* btn = new Wt::WPushButton("Apply", this);
		btn->clicked().connect(this, &Database::handleApply);
	}
	{
		Wt::WPushButton* btn = new Wt::WPushButton("Discard", this);
		btn->clicked().connect(this, &Database::loadSettings);
	}

	loadSettings();
}

void
Database::addDirectory(const std::string& path)
{
	int line = _table->rowCount();

	Wt::WLineEdit* lineEdit = new Wt::WLineEdit( path );
	_table->elementAt(line, 0)->addWidget( lineEdit );

	DirectoryValidator* validator = new DirectoryValidator(true);
	lineEdit->setValidator(validator);

	std::string status;

	if (!path.empty())
		status = validator->validate( path ).message().toUTF8();

	 Wt::WText* statusText = new Wt::WText( status );
	_table->elementAt(line, 1)->addWidget( statusText );
	_table->elementAt(line, 2)->addWidget( new Wt::WCheckBox() );

	lineEdit->blurred().connect(boost::bind(&Database::handleCheckDirectory, this, lineEdit, statusText));
}

void
Database::loadSettings(void)
{
	Wt::Dbo::Transaction transaction(_sessionData.getDatabaseHandler().getSession());

	// Get directories
	std::vector<::Database::MediaDirectory::pointer>	directories = ::Database::MediaDirectory::getAll(_sessionData.getDatabaseHandler().getSession());

	// delete rows
	assert(_table->rowCount() > 0 );
	for (int i = _table->rowCount() - 1; i > 0; --i)
		_table->deleteRow(i);

	BOOST_FOREACH(::Database::MediaDirectory::pointer directory, directories)
		addDirectory( directory->getPath().string() );

	// Get refresh settings
	::Database::MediaDirectorySettings::pointer settings = ::Database::MediaDirectorySettings::get(_sessionData.getDatabaseHandler().getSession());

	{
		int i;
		for (i = 0; i < _updatePeriodModel->rowCount(); ++i)
		{
			if (boost::any_cast<boost::posix_time::time_duration>
					(_updatePeriodModel->data(_updatePeriodModel->index(i, 0), Wt::UserRole))
					>= settings->getUpdatePeriod())
				break;
		}
		_updatePeriod->setCurrentIndex(i);
	}
	{
		int i;
		for (i = 0; i < _updateStartTimeModel->rowCount(); ++i)
		{
			if (boost::any_cast<boost::posix_time::time_duration>
					(_updateStartTimeModel->data(_updateStartTimeModel->index(i, 0), Wt::UserRole))
					>= settings->getUpdateStartTime())
				break;
		}
		_updateStartTime->setCurrentIndex(i);
	}

}

void
Database::handleAddDirectory(void)
{
	// TODO check if the previous entry is empty too
	addDirectory();
}

void
Database::handleDelDirectory()
{
	assert(_table->rowCount() > 0 );
	for (int i = _table->rowCount() - 1; i > 0; --i)
	{
		assert(_table->elementAt(i, 2)->count() == 1);
		Wt::WCheckBox *checkBox = dynamic_cast<Wt::WCheckBox*>(_table->elementAt(i, 2)->widget(0));

		if (checkBox->checkState() == Wt::CheckState::Checked)
			_table->deleteRow(i);
	}
}

void
Database::handleCheckDirectory(Wt::WLineEdit* edit, Wt::WText* status)
{
	Wt::WValidator* validator = edit->validator();
	if (validator)
		status->setText( validator->validate( edit->text().toUTF8() ).message().toUTF8());
}

bool
Database::saveSettings(void)
{
	Wt::Dbo::Transaction transaction(_sessionData.getDatabaseHandler().getSession());

	assert(_table->rowCount() > 0 );
	// Validate each directory in the table
	for (int i = 1; i < _table->rowCount(); ++i)
	{
		assert(_table->elementAt(i, 0)->count() == 1);

		Wt::WLineEdit* lineEdit = dynamic_cast<Wt::WLineEdit*>(_table->elementAt(i, 0)->widget(0));
		if (!lineEdit->validate())
			return false;;

	}

	// erase all directories and add from the table
	::Database::MediaDirectory::eraseAll( _sessionData.getDatabaseHandler().getSession() );

	for (int i = 1; i < _table->rowCount(); ++i)
	{
		Wt::WLineEdit* lineEdit = dynamic_cast<Wt::WLineEdit*>(_table->elementAt(i, 0)->widget(0));

		::Database::MediaDirectory::create( _sessionData.getDatabaseHandler().getSession(), lineEdit->text().toUTF8(), ::Database::MediaDirectory::Audio);
	}

	::Database::MediaDirectorySettings::pointer settings = ::Database::MediaDirectorySettings::get(_sessionData.getDatabaseHandler().getSession() );
	{
		int row = _updateStartTime->currentIndex();
		settings.modify()->setUpdateStartTime( boost::any_cast<boost::posix_time::time_duration>( _updateStartTimeModel->data(_updateStartTimeModel->index(row, 0), Wt::UserRole)));
	}
	{
		int row = _updatePeriod->currentIndex();
		settings.modify()->setUpdatePeriod( boost::any_cast<boost::posix_time::time_duration>( _updatePeriodModel->data(_updatePeriodModel->index(row, 0), Wt::UserRole)));
	}


	return true;
}

void
Database::handleApply(void)
{
	if (saveSettings())
	{
		boost::lock_guard<boost::mutex> serviceLock (ServiceManager::instance().mutex());

		DatabaseUpdateService::pointer service = ServiceManager::instance().getService<DatabaseUpdateService>();
		if (service)
			service->restart();

		// Show a popup to say it's fine!
		Wt::WMessageBox *messageBox = new Wt::WMessageBox
			("Status",
			 "New settings applied!",
			 Wt::Information, Wt::Ok);

		messageBox->setModal(true);

		messageBox->buttonClicked().connect(std::bind([=] ()
		{
			delete messageBox;
		}
		));

		messageBox->show();
	}
	else
	{
		Wt::WMessageBox *messageBox = new Wt::WMessageBox
			("Error",
			 "Cannot apply settings",
			 Wt::Critical, Wt::Ok);

		messageBox->setModal(true);

		messageBox->buttonClicked().connect(std::bind([=] ()
		{
			delete messageBox;
		}
		));

		messageBox->show();
	}

}

void
Database::handleScanNow(void)
{

}




} // namespace Settings
} // namespace UserInterface


