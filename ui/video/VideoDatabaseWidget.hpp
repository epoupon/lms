#ifndef VIDEO_DB_WIDGET_HPP
#define VIDEO_DB_WIDGET_HPP

#include <Wt/WTable>
#include <Wt/WContainerWidget>

#include "common/SessionData.hpp"
#include "database/FileTypes.hpp"

namespace UserInterface {

class VideoDatabaseWidget : public Wt::WContainerWidget
{
	public:
		VideoDatabaseWidget( DatabaseHandler& db, Wt::WContainerWidget *parent = 0);

		// Signals
		Wt::Signal< boost::filesystem::path >&	playVideo() { return _playVideo; }

	private:


		void updateView(Database::Path::pointer directory);
		void handleOpenDirectory(boost::filesystem::path directory);
		void handlePlayVideo(const boost::filesystem::path& path);

		void addHeader(void);
		void addDirectory(const std::string& name, const boost::filesystem::path& path);
		void addVideo(const std::string& name, const boost::posix_time::time_duration& duration, const boost::filesystem::path& path);

		DatabaseHandler&	_db;

		Wt::Signal< boost::filesystem::path > _playVideo;

		Wt::WTable*	_table;
};

} // namespace UserInterface

#endif

