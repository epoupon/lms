#ifndef UI_SETTINGS_FIRST_CONNECTION_FORM_VIEW_HPP
#define UI_SETTINGS_FIRST_CONNECTION_FORM_VIEW_HPP

#include <Wt/WContainerWidget>
#include <Wt/WTemplateFormView>
#include <Wt/WText>
#include <Wt/WPushButton>

#include "common/SessionData.hpp"

namespace UserInterface {
namespace Settings {

class FirstConnectionFormModel;

class FirstConnectionFormView : public Wt::WTemplateFormView
{
	public:
		FirstConnectionFormView(SessionData& sessionData, Wt::WContainerWidget *parent = 0);

	private:

		void processSave();

		Wt::WPushButton*		_saveButton;
		FirstConnectionFormModel*	_model;
		Wt::WText*             		_applyInfo;

};


} // namespace Settings
} // namespace UserInterface

#endif
