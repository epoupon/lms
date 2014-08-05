#ifndef UI_SETTINGS_MEDIA_DIR_FORM_VIEW_HPP
#define UI_SETTINGS_MEDIA_DIR_FORM_VIEW_HPP

#include <Wt/WContainerWidget>
#include <Wt/WTemplateFormView>
#include <Wt/WText>
#include <Wt/WSignal>

#include "common/SessionData.hpp"

namespace UserInterface {
namespace Settings {

class MediaDirectoryFormModel;

class MediaDirectoryFormView : public Wt::WTemplateFormView
{
	public:
		MediaDirectoryFormView(Database::Handler& db, Wt::WContainerWidget *parent = 0);

		// Signal emitted once the form is completed
		Wt::Signal<bool>&	completed()	{ return _sigCompleted; }

	private:
		void processCancel();	// reload from DB
		void processSave(); 	// commit into DB

		Wt::Signal<bool>		_sigCompleted;
		MediaDirectoryFormModel*	_model;
		Wt::WText*			_applyInfo;

};

} // namespace Settings
} // namespace UserInterface

#endif
