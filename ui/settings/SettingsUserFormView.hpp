#ifndef UI_SETTINGS_USER_FORM_VIEW_HPP
#define UI_SETTINGS_USER_FORM_VIEW_HPP

#include <Wt/WContainerWidget>
#include <Wt/WTemplateFormView>
#include <Wt/WSignal>

#include "common/SessionData.hpp"

namespace UserInterface {
namespace Settings {

class UserFormModel;

class UserFormView : public Wt::WTemplateFormView
{
	public:

		UserFormView(SessionData& sessionData, std::string userId, Wt::WContainerWidget *parent = 0);

		// Signal emitted once the form is completed
		Wt::Signal<bool>&	completed()	{ return _sigCompleted; }

	private:

		Wt::Signal<bool>	_sigCompleted;

		void processSave();
		void processCancel();

		UserFormModel*	_model;

};

} // namespace Settings
} // namespace UserInterface

#endif
