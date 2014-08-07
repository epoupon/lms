#ifndef UI_SETTINGS_MEDIA_DIRECTORIES_HPP
#define UI_SETTINGS_MEDIA_DIRECTORIES_HPP

#include <Wt/WStackedWidget>
#include <Wt/WContainerWidget>
#include <Wt/WTable>
#include <Wt/WSignal>

#include "database/MediaDirectory.hpp"

#include "common/SessionData.hpp"

namespace UserInterface {
namespace Settings {

class MediaDirectories : public Wt::WContainerWidget
{
	public:
		MediaDirectories(SessionData& sessioNData, Wt::WContainerWidget *parent = 0);

		void refresh();

		Wt::Signal<void>& changed()	{ return _sigChanged; }

	private:

		Wt::Signal<void>	_sigChanged;

		void handleMediaDirectoryFormCompleted(bool changed);

		void handleDelMediaDirectory(boost::filesystem::path p, Database::MediaDirectory::Type type);
		void handleCreateMediaDirectory(void);

		Database::Handler&	_db;

		Wt::WStackedWidget*	_stack;
		Wt::WTable*		_table;
};

} // namespace UserInterface
} // namespace Settings

#endif

