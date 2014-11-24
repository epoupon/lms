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
	_queryModel.setQuery(_db.getSession().query<ResultType>("select track from track").orderBy("track.artist_name,track.date,track.release_name,track.disc_number,track.track_number"));
	_queryModel.addColumn( "track.artist_name", "Artist" );
	_queryModel.addColumn( "track.release_name", "Album" );
	_queryModel.addColumn( "track.disc_number", "Disc #" );
	_queryModel.addColumn( "track.track_number", "Track #" );
	_queryModel.addColumn( "track.name", "Track" );
	_queryModel.addColumn( "track.duration", "Duration" );
	_queryModel.addColumn( "track.date", "Date" );
	_queryModel.addColumn( "track.original_date", "Original Date" );
	_queryModel.addColumn( "track.genre_list", "Genres" );

	_queryModel.setBatchSize(500);

	this->setSortingEnabled(true);
	this->setSelectionMode(Wt::ExtendedSelection);
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

	// If an item is double clicked, select the track and emit signal
	this->doubleClicked().connect( std::bind([=] (Wt::WModelIndex idx, Wt::WMouseEvent evt)
	{
		if (!idx.isValid())
			return;

		Wt::WModelIndexSet indexSet;
		indexSet.insert(idx);

		this->setSelectedIndexes( indexSet );

		_sigTrackDoubleClicked.emit( );

	}, std::placeholders::_1, std::placeholders::_2));

}

// Set constraints created by parent filters
void
TrackView::refresh(const Constraint& constraint)
{

	SqlQuery sqlQuery;

	sqlQuery.select( "track" );
	sqlQuery.from().And( FromClause("track"));
	sqlQuery.where().And(constraint.where);

	LMS_LOG(MOD_UI, SEV_DEBUG) << "TRACK REQ = '" << sqlQuery.get() << "'";

	Wt::Dbo::Query<ResultType> query = _db.getSession().query<ResultType>( sqlQuery.get() );

	query.groupBy("track").orderBy("track.artist_name,track.date,track.release_name,track.disc_number,track.track_number");

	BOOST_FOREACH(const std::string& bindArg, sqlQuery.where().getBindArgs()) {
		LMS_LOG(MOD_UI, SEV_DEBUG) << "Binding value '" << bindArg << "'";
		query.bind(bindArg);
	}

	_queryModel.setQuery( query, true );

}

void
TrackView::getSelectedTracks(std::vector<Database::Track::id_type>& track_ids)
{
	LMS_LOG(MOD_UI, SEV_DEBUG) << "Getting selected tracks...";

	Wt::WModelIndexSet indexSet = this->selectedIndexes();

	BOOST_FOREACH(Wt::WModelIndex index, indexSet)
	{
		if (!index.isValid())
			continue;

		const ResultType& result = _queryModel.resultRow( index.row() );

		// Get the track part
		Wt::Dbo::ptr<Database::Track> track ( result.get<0>() );

		track_ids.push_back(track.id());
	}

	LMS_LOG(MOD_UI, SEV_DEBUG) << "Getting all selected tracks DONE...";
}

void
TrackView::getTracks(std::vector<Database::Track::id_type>& trackIds)
{
	LMS_LOG(MOD_UI, SEV_DEBUG) << "Getting all tracks...";

	for (int i = 0; i < _queryModel.rowCount(); ++i)
	{
		const ResultType& result = _queryModel.resultRow( i );

		// Get the track part
		Wt::Dbo::ptr<Database::Track> track ( result.get<0>() );

		trackIds.push_back(track.id());
	}

	LMS_LOG(MOD_UI, SEV_DEBUG) << "Getting all tracks done!";
}

} // namespace UserInterface

