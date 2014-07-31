#include <Wt/WLineEdit>
#include <Wt/WString>
#include <Wt/WPushButton>
#include <Wt/WCheckBox>
#include <Wt/WComboBox>
#include <Wt/WBreak>

#include <Wt/WFormModel>
#include <Wt/WStringListModel>

#include "database/MediaDirectory.hpp"

#include "service/ServiceManager.hpp"
#include "service/DatabaseUpdateService.hpp"

#include "common/DirectoryValidator.hpp"

#include "SettingsDatabaseFormView.hpp"

namespace UserInterface {
namespace Settings {

class DatabaseFormModel : public Wt::WFormModel
{
	public:
		// Associate each field with a unique string literal.
		static const Field PathField;
		static const Field UpdatePeriodField;
		static const Field UpdateStartTimeField;
		static const Field UpdateRequestImmediateField;

		DatabaseFormModel(SessionData& sessionData, Wt::WObject *parent = 0)
			: Wt::WFormModel(parent),
			_sessionData(sessionData)
		{
			initializeModels();

			addField(PathField);
			addField(UpdatePeriodField);
			addField(UpdateStartTimeField);
			addField(UpdateRequestImmediateField);

			setValidator(PathField, 			createPathValidator());
			setValidator(UpdatePeriodField, 		createUpdatePeriodValidator());
			setValidator(UpdateStartTimeField, 		createStartTimeValidator());
			setValidator(UpdateRequestImmediateField,	createRequestImmediateFieldValidator());

			// populate the model with initial data
			loadData();
		}

		Wt::WAbstractItemModel *updatePeriodModel() { return _updatePeriodModel; }
		Wt::WAbstractItemModel *updateStartTimeModel() { return _updateStartTimeModel; }

		void loadData()
		{
			Wt::Dbo::Transaction transaction(_sessionData.getDatabaseHandler().getSession());

			// Get directories
			std::vector<::Database::MediaDirectory::pointer>	directories = ::Database::MediaDirectory::getAll(_sessionData.getDatabaseHandler().getSession());

			BOOST_FOREACH(::Database::MediaDirectory::pointer directory, directories)
			{
			        setValue(PathField, directory->getPath().string() );
				// TODO, handle multiple directories
				break;
			}

			// Get refresh settings
			::Database::MediaDirectorySettings::pointer settings = ::Database::MediaDirectorySettings::get(_sessionData.getDatabaseHandler().getSession());

			int periodRow = getUpdatePeriodModelRow( settings->getUpdatePeriod() );
			if (periodRow != -1)
				setValue(UpdatePeriodField, updatePeriod(periodRow));

			int startTimeRow = getUpdateStartTimeModelRow( settings->getUpdateStartTime() );
			if (startTimeRow != -1)
				setValue(UpdateStartTimeField, updateStartTime( startTimeRow ) );

			setValue(UpdateRequestImmediateField, settings->getManualScanRequested() );
		}

		void saveData()
		{
			Wt::Dbo::Session& session( _sessionData.getDatabaseHandler().getSession());
			Wt::Dbo::Transaction transaction(session);

			::Database::MediaDirectory::eraseAll( session );
			::Database::MediaDirectory::create(session, valueText(PathField).toUTF8(), ::Database::MediaDirectory::Audio);

			::Database::MediaDirectorySettings::pointer settings = ::Database::MediaDirectorySettings::get(_sessionData.getDatabaseHandler().getSession() );

			int periodRow = getUpdatePeriodModelRow( boost::any_cast<Wt::WString>(value(UpdatePeriodField)));
			assert(periodRow != -1);
			settings.modify()->setUpdatePeriod( updatePeriodDuration( periodRow ) );

			int startTimeRow = getUpdateStartTimeModelRow( boost::any_cast<Wt::WString>(value(UpdateStartTimeField)));
			assert(startTimeRow != -1);
			settings.modify()->setUpdateStartTime( updateStartTimeDuration( startTimeRow ) );

			settings.modify()->setManualScanRequested( boost::any_cast<bool>(value(UpdateRequestImmediateField )) );
		}


		int getUpdatePeriodModelRow(Wt::WString value)
		{
			for (int i = 0; i < _updatePeriodModel->rowCount(); ++i)
			{
				if (updatePeriod(i) == value)
					return i;
			}
			return -1;
		}

		int getUpdatePeriodModelRow(boost::posix_time::time_duration duration)
		{
			for (int i = 0; i < _updatePeriodModel->rowCount(); ++i)
			{
				if (updatePeriodDuration(i) == duration)
					return i;
			}

			return -1;
		}

		boost::posix_time::time_duration updatePeriodDuration(int row) {
			return boost::any_cast<boost::posix_time::time_duration>
				(_updatePeriodModel->data(_updatePeriodModel->index(row, 0), Wt::UserRole));
		}

		Wt::WString updatePeriod(int row) {
			return boost::any_cast<Wt::WString>
				(_updatePeriodModel->data(_updatePeriodModel->index(row, 0), Wt::DisplayRole));
		}


		int getUpdateStartTimeModelRow(Wt::WString value)
		{
			for (int i = 0; i < _updateStartTimeModel->rowCount(); ++i)
			{
				if (updateStartTime(i) == value)
					return i;
			}

			return -1;
		}

		int getUpdateStartTimeModelRow(boost::posix_time::time_duration duration)
		{
			for (int i = 0; i < _updateStartTimeModel->rowCount(); ++i)
			{
				if (updateStartTimeDuration(i) == duration)
					return i;
			}

			return -1;
		}

		boost::posix_time::time_duration updateStartTimeDuration(int row) {
			return boost::any_cast<boost::posix_time::time_duration>
				(_updateStartTimeModel->data(_updateStartTimeModel->index(row, 0), Wt::UserRole));
		}

		Wt::WString updateStartTime(int row) {
			return boost::any_cast<Wt::WString>
				(_updateStartTimeModel->data(_updateStartTimeModel->index(row, 0), Wt::DisplayRole));
		}


	private:

		void initializeModels() {

			_updatePeriodModel = new Wt::WStringListModel(this);

			_updatePeriodModel->addString("Never");
			_updatePeriodModel->setData(0, 0, boost::posix_time::time_duration( boost::posix_time::hours(0) ), Wt::UserRole);

			_updatePeriodModel->addString("Daily");
			_updatePeriodModel->setData(1, 0, boost::posix_time::time_duration( boost::posix_time::hours(24) ), Wt::UserRole);

			_updatePeriodModel->addString("Weekly");
			_updatePeriodModel->setData(2, 0, boost::posix_time::time_duration( boost::posix_time::hours(24*7) ), Wt::UserRole);

			_updatePeriodModel->addString("Monthly");
			_updatePeriodModel->setData(3, 0, boost::posix_time::time_duration(boost::posix_time::hours(24*30) ), Wt::UserRole);


			_updateStartTimeModel = new Wt::WStringListModel(this);

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

		}

		Wt::WValidator *createPathValidator() {
			DirectoryValidator* v = new DirectoryValidator();
			v->setMandatory(true);
			return v;
		}

		Wt::WValidator *createUpdatePeriodValidator() {
			Wt::WValidator* v = new Wt::WValidator();
			v->setMandatory(true);
			return v;
		}

		Wt::WValidator *createStartTimeValidator() {
			Wt::WValidator* v = new Wt::WValidator();
			v->setMandatory(true);
			return v;
		}

		Wt::WValidator *createRequestImmediateFieldValidator() {
			Wt::WValidator* v = new Wt::WValidator();
			return v;
		}

		SessionData&		_sessionData;
		Wt::WStringListModel*	_updatePeriodModel;
		Wt::WStringListModel*	_updateStartTimeModel;

};

const Wt::WFormModel::Field DatabaseFormModel::PathField 			= "path";
const Wt::WFormModel::Field DatabaseFormModel::UpdatePeriodField		= "update-period";
const Wt::WFormModel::Field DatabaseFormModel::UpdateStartTimeField		= "update-start-time";
const Wt::WFormModel::Field DatabaseFormModel::UpdateRequestImmediateField	= "update-request-immediate-scan";


DatabaseFormView::DatabaseFormView(SessionData& sessionData, Wt::WContainerWidget *parent)
: Wt::WTemplateFormView(parent)
{
	model = new DatabaseFormModel(sessionData, this);

	setTemplateText(tr("databaseForm-template"));
	addFunction("id", &WTemplate::Functions::id);
	addFunction("block", &WTemplate::Functions::id);

	applyInfo = new Wt::WText();
	applyInfo->setInline(false);
	applyInfo->hide();
	bindWidget("apply-info", applyInfo);

	// Path
	Wt::WLineEdit *pathEdit = new Wt::WLineEdit();
	setFormWidget(DatabaseFormModel::PathField, pathEdit);
	pathEdit->changed().connect(applyInfo, &Wt::WWidget::hide);

	// Update Period
	Wt::WComboBox *updatePeriodCB = new Wt::WComboBox();
	setFormWidget(DatabaseFormModel::UpdatePeriodField, updatePeriodCB);
	updatePeriodCB->setModel(model->updatePeriodModel());
	updatePeriodCB->changed().connect(applyInfo, &Wt::WWidget::hide);

	// Update Start Time
	Wt::WComboBox *updateStartTimeCB = new Wt::WComboBox();
	setFormWidget(DatabaseFormModel::UpdateStartTimeField, updateStartTimeCB);
	updateStartTimeCB->setModel(model->updateStartTimeModel());
	updateStartTimeCB->changed().connect(applyInfo, &Wt::WWidget::hide);

	// Request Immediate scan
	Wt::WCheckBox *immScan = new Wt::WCheckBox();
	setFormWidget(DatabaseFormModel::UpdateRequestImmediateField, immScan);
	immScan->changed().connect(applyInfo, &Wt::WWidget::hide);

	// Title & Buttons
	bindString("title", "Media directories settings");

	Wt::WPushButton *saveButton = new Wt::WPushButton("Apply");
	bindWidget("apply-button", saveButton);
	saveButton->setStyleClass("btn-primary");

	Wt::WPushButton *discardButton = new Wt::WPushButton("Discard");
	bindWidget("discard-button", discardButton);

	saveButton->clicked().connect(this, &DatabaseFormView::processSave);
	discardButton->clicked().connect(this, &DatabaseFormView::processDiscard);

	updateView(model);

}

void
DatabaseFormView::processDiscard()
{
	applyInfo->show();

	applyInfo->setText( Wt::WString::fromUTF8("Parameters reverted!"));
	applyInfo->setStyleClass("alert alert-info");

	model->loadData();

	model->validate();
	updateView(model);
}

void
DatabaseFormView::processSave()
{
	updateModel(model);

	applyInfo->show();

	if (model->validate()) {
		// Make the model to commit data into DB
		model->saveData();

		// Restarting the update service
		{
			boost::lock_guard<boost::mutex> serviceLock (ServiceManager::instance().mutex());

			DatabaseUpdateService::pointer service = ServiceManager::instance().getService<DatabaseUpdateService>();
			if (service)
				service->restart();
		}

		applyInfo->setText( Wt::WString::fromUTF8("New parameters successfully applied!"));
		applyInfo->setStyleClass("alert alert-success");
	}
	else {
		applyInfo->setText( Wt::WString::fromUTF8("Cannot apply new parameters!"));
		applyInfo->setStyleClass("alert alert-danger");
	}
	// Udate the view: Delete any validation message in the view, etc.
	updateView(model);
}


} // namespace Settings
} // namespace UserInterface


