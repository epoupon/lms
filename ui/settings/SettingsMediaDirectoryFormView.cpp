#include <vector>

#include <Wt/WStringListModel>
#include <Wt/WLineEdit>
#include <Wt/WFormModel>
#include <Wt/WComboBox>
#include <Wt/WPushButton>

#include "database/MediaDirectory.hpp"

#include "common/DirectoryValidator.hpp"


#include "SettingsMediaDirectoryFormView.hpp"

namespace UserInterface {
namespace Settings {

// TODO validate if directory already exists
class MediaDirectoryFormModel : public Wt::WFormModel
{
	public:

		// Associate each field with a unique string literal.
		static const Field PathField;
		static const Field TypeField;

		MediaDirectoryFormModel(Database::Handler& db, Wt::WObject *parent = 0)
			: Wt::WFormModel(parent),
			_db(db)
		{
			initializeModels();

			addField(PathField);
			addField(TypeField);

			DirectoryValidator* dirValidator = new DirectoryValidator();
			dirValidator->setMandatory(true);
			setValidator(PathField, dirValidator);
			setValidator(TypeField, new Wt::WValidator(true)); // mandatory

		}

		Wt::WAbstractItemModel *typeModel() { return _typeModel; }

		bool saveData(Wt::WString& error)
		{
			try {
				Wt::Dbo::Transaction transaction(_db.getSession());

				Database::MediaDirectory::Type type
					= (valueText(TypeField) == "Audio") ? Database::MediaDirectory::Audio : Database::MediaDirectory::Video;

				if (Database::MediaDirectory::get(_db.getSession(), valueText(PathField).toUTF8(), type))
				{
					error = "This Path/Type already exists!";
					return false;
				}

				Database::MediaDirectory::create(_db.getSession(), valueText(PathField).toUTF8(), type);

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
			_typeModel = new Wt::WStringListModel(this);
			_typeModel->addString("Audio");
			_typeModel->addString("Video");
		}

		Database::Handler&	_db;
		std::string		_userId;
		Wt::WStringListModel*	_typeModel;
};

const Wt::WFormModel::Field MediaDirectoryFormModel::PathField = "path";
const Wt::WFormModel::Field MediaDirectoryFormModel::TypeField = "type";

MediaDirectoryFormView::MediaDirectoryFormView(Database::Handler& db, Wt::WContainerWidget *parent)
: Wt::WTemplateFormView(parent)
{

	_model = new MediaDirectoryFormModel(db, this);

	setTemplateText(tr("mediaDirectoryForm-template"));
	addFunction("id", &WTemplate::Functions::id);
	addFunction("block", &WTemplate::Functions::id);

	_applyInfo = new Wt::WText();
	_applyInfo->setInline(false);
	_applyInfo->hide();
	bindWidget("apply-info", _applyInfo);

	// Path
	Wt::WLineEdit* pathEdit = new Wt::WLineEdit();
	setFormWidget(MediaDirectoryFormModel::PathField, pathEdit);
	pathEdit->changed().connect(_applyInfo, &Wt::WWidget::hide);

	// Type
	Wt::WComboBox *typeCB = new Wt::WComboBox();
	setFormWidget(MediaDirectoryFormModel::TypeField, typeCB);
	typeCB->setModel(_model->typeModel());
	typeCB->changed().connect(_applyInfo, &Wt::WWidget::hide);

	// Title & Buttons
	bindString("title", "Add Media Folder");

	Wt::WPushButton *saveButton = new Wt::WPushButton("Add");
	saveButton->setStyleClass("btn-primary");
	bindWidget("save-button", saveButton);
	saveButton->clicked().connect(this, &MediaDirectoryFormView::processSave);

	Wt::WPushButton *cancelButton = new Wt::WPushButton("Cancel");
	bindWidget("cancel-button", cancelButton);
	cancelButton->clicked().connect(this, &MediaDirectoryFormView::processCancel);

	updateView(_model);
}

void
MediaDirectoryFormView::processCancel()
{
	// parent widget will delete this widget
	completed().emit(false);
}

void
MediaDirectoryFormView::processSave()
{
	updateModel(_model);

	_applyInfo->show();
	if (_model->validate())
	{
		Wt::WString error;
		// commit model into DB
		if (_model->saveData(error) ) {
			completed().emit(true);
			return;
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
