#ifndef TRACK_WIDGET_HPP
#define TRACK_WIDGET_HPP

#include <Wt/WTableView>
#include <Wt/Dbo/QueryModel>

#include "database/DatabaseHandler.hpp"
#include "database/AudioTypes.hpp"

#include "FilterWidget.hpp"

namespace UserInterface {

class TrackWidget : public FilterWidget
{
	public:

		TrackWidget( DatabaseHandler& db, Wt::WContainerWidget* parent = 0);

		// Set constraints created by parent filters
		void refresh(const Constraint& constraint);

		// Create constraints for child filters (N/A)
		void getConstraint(Constraint& constraint) {}

		void selectNextTrack(void);	// Emit a trackSelected() if success

		// Signals
		Wt::Signal< boost::filesystem::path >& trackSelected() { return _trackSelected; }

	private:

		void handleTrackSelected();

		Wt::Signal< boost::filesystem::path > _trackSelected;

		DatabaseHandler&                        _db;

		typedef boost::tuple<Database::Track::pointer, Database::Release::pointer, Database::Artist::pointer>  ResultType;
		Wt::Dbo::QueryModel< ResultType > _queryModel;
		Wt::WTableView* _tableView;

		void updateStats(void);

		Wt::WText*	_trackStats;

};

} // namespace UserInterface

#endif

