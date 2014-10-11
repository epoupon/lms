/*
 * Copyright (C) 2013 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <boost/foreach.hpp>

#include <Wt/WItemDelegate>
#include <Wt/WBreak>

#include "logger/Logger.hpp"

#include "TrackWidget.hpp"

namespace UserInterface {

TrackWidget::TrackWidget( Database::Handler& db, Wt::WContainerWidget* parent)
: FilterWidget( parent ),
_db(db),
_tableView(nullptr),
_trackStats(nullptr)
{

	// ----- TRACK -----
	_queryModel.setQuery(_db.getSession().query<ResultType>("select track,release,artist from track,release,artist where track.release_id = release.id and track.artist_id = artist.id" ).orderBy("artist.name,release.name,track.disc_number,track.track_number"));
	_queryModel.addColumn( "artist.name", "Artist" );
	_queryModel.addColumn( "release.name", "Album" );
	_queryModel.addColumn( "track.disc_number", "Disc #" );
	_queryModel.addColumn( "track.track_number", "Track #" );
	_queryModel.addColumn( "track.name", "Track" );
	_queryModel.addColumn( "track.duration", "Duration" );
	_queryModel.addColumn( "track.date", "Date" );
	_queryModel.addColumn( "track.original_date", "Original Date" );
	_queryModel.addColumn( "track.genre_list", "Genres" );

	_queryModel.setBatchSize(1000);

	_tableView = new Wt::WTableView( this );
	_tableView->resize(Wt::WLength::Auto, 400);

	_tableView->setSortingEnabled(true);
	_tableView->setSelectionMode(Wt::SingleSelection);
	_tableView->setAlternatingRowColors(true);
	_tableView->setModel(&_queryModel);

	// Duration display
	{
		// TODO better handle 1 hour+ files!
		Wt::WItemDelegate *delegate = new Wt::WItemDelegate(this);
		delegate->setTextFormat("mm:ss");
		_tableView->setItemDelegateForColumn(5, delegate);
	}

	// Date display, just the year
	{
		Wt::WItemDelegate *delegate = new Wt::WItemDelegate(this);
		delegate->setTextFormat("yyyy");
		_tableView->setItemDelegateForColumn(6, delegate);
	}
	{
		Wt::WItemDelegate *delegate = new Wt::WItemDelegate(this);
		delegate->setTextFormat("yyyy");
		_tableView->setItemDelegateForColumn(7, delegate);
	}

	// TODO other event!
	_tableView->selectionChanged().connect(this, &TrackWidget::handleTrackSelected);

	new Wt::WBreak(this);
	_trackStats = new Wt::WText(this);

	updateStats();
}

void
TrackWidget::updateStats(void)
{
	std::ostringstream oss; oss <<  "Files :" << _tableView->model()->rowCount();
	// TODO from UTF-8 ?
	_trackStats->setText(oss.str());
}

// Set constraints created by parent filters
void
TrackWidget::refresh(const Constraint& constraint)
{

	SqlQuery sqlQuery;

	sqlQuery.select( "track,release,artist" );
	sqlQuery.from().And( FromClause("artist,release,track,genre,track_genre"));
	sqlQuery.where().And(constraint.where);

	LMS_LOG(MOD_UI, SEV_DEBUG) << "TRACK REQ = '" << sqlQuery.get() << "'";

	Wt::Dbo::Query<ResultType> query = _db.getSession().query<ResultType>( sqlQuery.get() );

	query.groupBy("track").orderBy("artist.name,track.date,release.name,track.disc_number,track.track_number");

	BOOST_FOREACH(const std::string& bindArg, sqlQuery.where().getBindArgs()) {
		LMS_LOG(MOD_UI, SEV_DEBUG) << "Binding value '" << bindArg << "'";
		query.bind(bindArg);
	}

	_queryModel.setQuery( query, true );

	updateStats();
}

void
TrackWidget::handleTrackSelected(void)
{
	Wt::WModelIndexSet indexSet = _tableView->selectedIndexes();
	if (!indexSet.empty()) {
		Wt::WModelIndex currentIndex( *indexSet.begin() );

		// Check there are remainin tracks!
		if (currentIndex.isValid())
		{
			ResultType result = _queryModel.resultRow( currentIndex.row());

			// Get the track part
			Wt::Dbo::ptr<Database::Track> track ( result.get<0>() );

			_trackSelected.emit( track->getPath() );
		}

	}
}

void
TrackWidget::selectNextTrack(void)
{
	Wt::WModelIndexSet indexSet = _tableView->selectedIndexes();
	if (!indexSet.empty()) {
		Wt::WModelIndex currentIndex( *indexSet.begin() );
		// Check there are remainin tracks!
		if (currentIndex.isValid() && _tableView->model()->rowCount() > currentIndex.row() + 1)
		{
			_tableView->select( _tableView->model()->index( currentIndex.row() + 1, currentIndex.column()));

			ResultType result = _queryModel.resultRow( currentIndex.row() + 1 );

			// Get the track part
			Wt::Dbo::ptr<Database::Track> track ( result.get<0>() );

			_trackSelected.emit( track->getPath() );
		}
	}
}

} // namespace UserInterface

