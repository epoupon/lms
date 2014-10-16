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

#include "TrackView.hpp"

namespace UserInterface {

TrackView::TrackView( Database::Handler& db, Wt::WContainerWidget* parent)
: Wt::WTableView( parent ),
_db(db)
{
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

	_queryModel.setBatchSize(250);

	this->setSortingEnabled(true);
	this->setSelectionMode(Wt::SingleSelection);
	this->setAlternatingRowColors(true);
	this->setModel(&_queryModel);

	this->setColumnWidth(0, 180);	// Artist
	this->setColumnWidth(1, 180);	// Album
	this->setColumnWidth(2, 70);	// Disc Number
	this->setColumnWidth(3, 70);	// Track Number
	this->setColumnWidth(4, 180);	// Track
	this->setColumnWidth(5, 70);	// Duration
	this->setColumnWidth(6, 70);	// Date
	this->setColumnWidth(7, 70);	// Original Date
	this->setColumnWidth(8, 180);	// Genres

//	this->setOverflow(Wt::WContainerWidget::OverflowScroll, Wt::Vertical);

	// Duration display
	{
		// TODO better handle 1 hour+ files!
		Wt::WItemDelegate *delegate = new Wt::WItemDelegate(this);
		delegate->setTextFormat("mm:ss");
		this->setItemDelegateForColumn(5, delegate);
	}

	// Date display, just the year
	{
		Wt::WItemDelegate *delegate = new Wt::WItemDelegate(this);
		delegate->setTextFormat("yyyy");
		this->setItemDelegateForColumn(6, delegate);
	}
	{
		Wt::WItemDelegate *delegate = new Wt::WItemDelegate(this);
		delegate->setTextFormat("yyyy");
		this->setItemDelegateForColumn(7, delegate);
	}

	// TODO other event!
	this->selectionChanged().connect(this, &TrackView::handleTrackSelected);
}

// Set constraints created by parent filters
void
TrackView::refresh(const Constraint& constraint)
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

}

void
TrackView::handleTrackSelected(void)
{
	Wt::WModelIndexSet indexSet = this->selectedIndexes();
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
TrackView::selectNextTrack(void)
{
	Wt::WModelIndexSet indexSet = this->selectedIndexes();
	if (!indexSet.empty()) {
		Wt::WModelIndex currentIndex( *indexSet.begin() );
		// Check there are remainin tracks!
		if (currentIndex.isValid() && this->model()->rowCount() > currentIndex.row() + 1)
		{
			this->select( this->model()->index( currentIndex.row() + 1, currentIndex.column()));

			ResultType result = _queryModel.resultRow( currentIndex.row() + 1 );

			// Get the track part
			Wt::Dbo::ptr<Database::Track> track ( result.get<0>() );

			_trackSelected.emit( track->getPath() );
		}
	}
}

} // namespace UserInterface

