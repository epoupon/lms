#ifndef UI_SETTINGS_ACCOUNT_FORM_VIEW_HPP
#define UI_SETTINGS_ACCOUNT_FORM_VIEW_HPP

#include <Wt/WContainerWidget>
#include <Wt/WTemplateFormView>
#include <Wt/WText>

#include "common/SessionData.hpp"

namespace UserInterface {
namespace Settings {

class AccountFormModel;

class AccountFormView : public Wt::WTemplateFormView
{
	public:
		AccountFormView(SessionData& sessionData, std::string userId, Wt::WContainerWidget *parent = 0);

	private:
		void processCancel();	// reload from DB
		void processSave(); 	// commit into DB

		AccountFormModel*	_model;
		Wt::WText*		_applyInfo;

};

} // namespace Settings
} // namespace UserInterface

#endif
