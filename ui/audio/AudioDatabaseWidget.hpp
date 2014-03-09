#ifndef AUDIO_DB_WIDGET_HPP
#define AUDIO_DB_WIDGET_HPP

#include <boost/filesystem/path.hpp>

#include <Wt/WContainerWidget>
#include <Wt/WSignal>

#include "database/DatabaseHandler.hpp"

#include "FilterWidget.hpp"

class AudioDatabaseWidget : public Wt::WContainerWidget
{
	public:
		AudioDatabaseWidget( Wt::WContainerWidget *parent = 0);

		void search(const std::string& text);

		void selectNextTrack(void);				// Will later emit the next selected track

		// Signals
		Wt::Signal< boost::filesystem::path >& trackSelected() { return _trackSelected; }

	private:

		DatabaseHandler	_db;

		Wt::Signal< boost::filesystem::path > _trackSelected;

		void handleTrackSelected(boost::filesystem::path p);
		void handleFilterUpdated(std::size_t idFilter);

		std::vector<FilterWidget*>	_filters;	// Free Search, Genre, Artist, Release, etc.

		bool _refreshingFilters;

};


#endif

