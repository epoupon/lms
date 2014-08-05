#ifndef UI_SETTINGS_MEDIA_DIRECTORIES_HPP
#define UI_SETTINGS_MEDIA_DIRECTORIES_HPP

#include <Wt/WStackedWidget>
#include <Wt/WContainerWidget>
#include <Wt/WTable>

#include "common/SessionData.hpp"

namespace UserInterface {
namespace Settings {

class MediaDirectories : public Wt::WContainerWidget
{
	public:
		MediaDirectories(SessionData& sessioNData, Wt::WContainerWidget *parent = 0);

		void refresh();

	private:

		void handleMediaDirectoryFormCompleted(bool changed);

		void handleDelMediaDirectory(boost::filesystem::path p);
		void handleCreateMediaDirectory(void);

		Database::Handler&	_db;

		Wt::WStackedWidget*	_stack;
		Wt::WTable*		_table;
};

} // namespace UserInterface
} // namespace Settings

#endif

