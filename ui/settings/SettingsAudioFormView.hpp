#ifndef UI_SETTINGS_AUDIO_FORM_VIEW_HPP
#define UI_SETTINGS_AUDIO_ACCOUNT_FORM_VIEW_HPP

#include <Wt/WContainerWidget>
#include <Wt/WTemplateFormView>
#include <Wt/WText>

#include "common/SessionData.hpp"

namespace UserInterface {
namespace Settings {

class AudioFormModel;

class AudioFormView : public Wt::WTemplateFormView
{
	public:
		AudioFormView(SessionData& sessionData, std::string userId, Wt::WContainerWidget *parent = 0);

	private:
		void processCancel();	// reload from DB
		void processSave(); 	// commit into DB

		AudioFormModel*	_model;
		Wt::WText*	_applyInfo;

};

} // namespace Settings
} // namespace UserInterface

#endif
