#ifndef VIDEO_DB_WIDGET_HPP
#define VIDEO_DB_WIDGET_HPP

#include <Wt/WTable>
#include <Wt/WContainerWidget>

#include "common/SessionData.hpp"

namespace UserInterface {

class VideoDatabaseWidget : public Wt::WContainerWidget
{
	public:
		VideoDatabaseWidget( Database::Handler& db, Wt::WContainerWidget *parent = 0);

		// Signals
		Wt::Signal< boost::filesystem::path >&	playVideo() { return _playVideo; }

	private:

		void addHeader(void);
		void addDirectory(const std::string& name, boost::filesystem::path path, size_t depth);
		void addVideo(const std::string& name, const boost::posix_time::time_duration& duration, const boost::filesystem::path& path);

		void updateView(boost::filesystem::path directory, size_t depth);

		Database::Handler&	_db;

		Wt::Signal< boost::filesystem::path > _playVideo;

		Wt::WTable*	_table;
};

} // namespace UserInterface

#endif

