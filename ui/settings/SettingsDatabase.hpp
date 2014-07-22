#ifndef UI_SETTINGS_DB_HPP
#define UI_SETTINGS_DB_HPP

#include <Wt/WContainerWidget>
#include <Wt/WTable>
#include <Wt/WComboBox>
#include <Wt/WLineEdit>
#include <Wt/WStringListModel>

#include "common/SessionData.hpp"

namespace UserInterface {
namespace Settings {

class Database : public Wt::WContainerWidget
{
	public:
		Database(SessionData& sessionData, Wt::WContainerWidget *parent = 0);

	private:
		void handleApply();

		void handleCheckDirectory(Wt::WLineEdit* edit, Wt::WText* status);
		void handleAddDirectory();
		void handleDelDirectory();

		void handleScanNow(void);

		void loadSettings(void);	// load from db
		bool saveSettings(void);	// save into db

		void addDirectory(const std::string& path = "");

		SessionData& _sessionData;

		Wt::WTable*		_table;

		Wt::WComboBox*		_updatePeriod;
		Wt::WStringListModel* 	_updatePeriodModel;

		Wt::WComboBox*		_updateStartTime;
		Wt::WStringListModel* 	_updateStartTimeModel;
};


} // namespace Settings
} // namespace UserInterface

#endif

