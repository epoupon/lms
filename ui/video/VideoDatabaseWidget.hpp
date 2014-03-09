#ifndef VIDEO_DB_WIDGET_HPP
#define VIDEO_DB_WIDGET_HPP

#include <Wt/WTable>
#include <Wt/WContainerWidget>

#include "database/DatabaseHandler.hpp"
#include "database/FileTypes.hpp"

class VideoDatabaseWidget : public Wt::WContainerWidget
{
	public:
		VideoDatabaseWidget( Wt::WContainerWidget *parent = 0);

		// Signals
		Wt::Signal< boost::filesystem::path >&	playVideo() { return _playVideo; }

	private:


		void updateView(Path::pointer directory);
		void handleOpenDirectory(boost::filesystem::path directory);
		void handlePlayVideo(const boost::filesystem::path& path);

		void addHeader(void);
		void addDirectory(const std::string& name, const boost::filesystem::path& path);
		void addVideo(const std::string& name, const boost::posix_time::time_duration& duration, const boost::filesystem::path& path);

		Wt::Signal< boost::filesystem::path > _playVideo;

		DatabaseHandler	_db;

		Wt::WTable*	_table;
};


#endif

