#include <vector>

#include <Wt/WStringListModel>
#include <Wt/WFormModel>
#include <Wt/WComboBox>
#include <Wt/WPushButton>

#include "common/Validators.hpp"

#include "SettingsAudioFormView.hpp"

namespace UserInterface {
namespace Settings {

class AudioFormModel : public Wt::WFormModel
{
	public:

		// Associate each field with a unique string literal.
		static const Field BitrateField;

		AudioFormModel(SessionData& sessionData, std::string userId, Wt::WObject *parent = 0)
			: Wt::WFormModel(parent),
			_db(sessionData.getDatabaseHandler()),
			_userId(userId)
		{
			initializeModels();

			addField(BitrateField);

			setValidator(BitrateField, new Wt::WValidator(true)); // mandatory

			// populate the model with initial data
			loadData();
		}

		Wt::WAbstractItemModel *bitrateModel() { return _bitrateModel; }

		void loadData()
		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			Database::User::pointer user = Database::User::getById( _db.getSession(), _userId);

			if (user)
				setValue(BitrateField, std::min(user->getMaxAudioBitrate(), user->getAudioBitrate()) / 1000); // in kps
			else
				setValue(BitrateField, Wt::WString());
		}

		bool saveData(Wt::WString& error)
		{
			// DBO transaction active here
			try {
				Wt::Dbo::Transaction transaction(_db.getSession());

				Database::User::pointer user = Database::User::getById( _db.getSession(), _userId);
				// user may have been deleted by someone else
				if (user)
				{
					user.modify()->setAudioBitrate( Wt::asNumber(value(BitrateField)) * 1000); // in kbps
				}

			}
			catch(Wt::Dbo::Exception& exception)
			{
				std::cerr << "Dbo exception: " << exception.what() << std::endl;
				return false;
			}

			return true;
		}

	private:
		void initializeModels()
		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			Database::User::pointer user = Database::User::getById(_db.getSession(), _userId);

			_bitrateModel = new Wt::WStringListModel();
			BOOST_FOREACH(std::size_t bitrate, Database::User::audioBitrates)
			{
				if (user && bitrate <= user->getMaxAudioBitrate())
					_bitrateModel->addString( Wt::WString("{1}").arg( bitrate / 1000 ) ); // in kbps
			}
		}

		Database::Handler&	_db;
		std::string		_userId;
		Wt::WStringListModel*	_bitrateModel;
};

const Wt::WFormModel::Field AudioFormModel::BitrateField = "bitrate";

AudioFormView::AudioFormView(SessionData& sessionData, std::string userId, Wt::WContainerWidget *parent)
: Wt::WTemplateFormView(parent)
{

	_model = new AudioFormModel(sessionData, userId, this);

	setTemplateText(tr("audioForm-template"));
	addFunction("id", &WTemplate::Functions::id);
	addFunction("block", &WTemplate::Functions::id);

	_applyInfo = new Wt::WText();
	_applyInfo->setInline(false);
	_applyInfo->hide();
	bindWidget("apply-info", _applyInfo);

	// Bitrate
	Wt::WComboBox *bitrateCB = new Wt::WComboBox();
	setFormWidget(AudioFormModel::BitrateField, bitrateCB);
	bitrateCB->setStyleClass("span2");
	bitrateCB->setModel(_model->bitrateModel());
	bitrateCB->changed().connect(_applyInfo, &Wt::WWidget::hide);

	// Title & Buttons
	bindString("title", "Audio settings");

	Wt::WPushButton *saveButton = new Wt::WPushButton("Apply");
	saveButton->setStyleClass("btn-primary");
	bindWidget("save-button", saveButton);
	saveButton->clicked().connect(this, &AudioFormView::processSave);

	Wt::WPushButton *cancelButton = new Wt::WPushButton("Discard");
	bindWidget("cancel-button", cancelButton);
	cancelButton->clicked().connect(this, &AudioFormView::processCancel);


	updateView(_model);

}

void
AudioFormView::processCancel()
{
	_applyInfo->show();

	_applyInfo->setText( Wt::WString::fromUTF8("Parameters reverted!"));
	_applyInfo->setStyleClass("alert alert-info");
	_model->loadData();

	_model->validate();
	updateView(_model);
}

void
AudioFormView::processSave()
{
	updateModel(_model);

	_applyInfo->show();
	if (_model->validate())
	{
		Wt::WString error;
		// commit model into DB
		if (_model->saveData(error) )
		{
			_applyInfo->setText( Wt::WString::fromUTF8("New parameters successfully applied!"));
			_applyInfo->setStyleClass("alert alert-success");
		}
		else
		{
			_applyInfo->setText( Wt::WString::fromUTF8("Cannot apply new parameters: ") + error);
			_applyInfo->setStyleClass("alert alert-danger");
		}
	}
	else
	{
		_applyInfo->setText( Wt::WString::fromUTF8("Cannot apply new parameters!"));
		_applyInfo->setStyleClass("alert alert-danger");
	}
	updateView(_model);
}

} // namespace Settings
} // namespace UserInterface
