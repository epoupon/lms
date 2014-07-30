#ifndef UI_SETTINGS_DB_FORM_VIEW_HPP
#define UI_SETTINGS_DB_FORM_VIEW_HPP

#include <Wt/WContainerWidget>
#include <Wt/WTemplateFormView>
#include <Wt/WText>

#include "common/SessionData.hpp"

namespace UserInterface {
namespace Settings {

class DatabaseFormModel;

class DatabaseFormView : public Wt::WTemplateFormView
{
	public:
		DatabaseFormView(SessionData& sessionData, Wt::WContainerWidget *parent = 0);

	private:

		void processSave();
		void processDiscard();

		Wt::WText		*applyInfo;
		DatabaseFormModel	*model;

};


} // namespace Settings
} // namespace UserInterface

#endif

