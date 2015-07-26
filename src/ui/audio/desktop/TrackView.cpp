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

#include "LmsApplication.hpp"
#include "TrackView.hpp"

namespace UserInterface {
namespace Desktop {

TrackView::TrackView(Wt::WContainerWidget* parent)
: Wt::WTableView( parent )
{

	static const std::vector<Wt::WString> columnNames =
	{
		"Artist",
		"Album",
		"Disc #",
		"Track #",
		"Track",
		"Duration",
		"Date",
		"Original Date",
		"Genres",
	};

	Database::SearchFilter filter;

	Database::Track::updateUIQueryModel(DboSession(), _queryModel, filter, columnNames);

	_queryModel.setBatchSize(300);

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
TrackView::refresh(Database::SearchFilter& filter)
{
	Database::Track::updateUIQueryModel(DboSession(), _queryModel, filter);
}

void
TrackView::getSelectedTracks(std::vector<Database::Track::id_type>& track_ids)
{
	LMS_LOG(MOD_UI, SEV_DEBUG) << "Getting selected tracks...";

	Wt::WModelIndexSet indexSet = this->selectedIndexes();

	for (Wt::WModelIndex index : indexSet)
	{
		if (!index.isValid())
			continue;

		Database::Track::id_type id = _queryModel.resultRow( index.row() ).get<0>();

		track_ids.push_back(id);
	}

	LMS_LOG(MOD_UI, SEV_DEBUG) << "Getting all selected tracks: " << track_ids.size();
}

std::size_t
TrackView::getNbSelectedTracks(void)
{
	return this->selectedIndexes().size();
}

int
TrackView::getFirstSelectedTrackPosition(void)
{
	Wt::WModelIndexSet indexSet = this->selectedIndexes();

	for (Wt::WModelIndex index : indexSet)
	{
		if (!index.isValid())
			continue;

		return index.row();
	}

	return 0;
}

void
TrackView::getTracks(std::vector<Database::Track::id_type>& trackIds)
{
	LMS_LOG(MOD_UI, SEV_DEBUG) << "Getting all tracks...";

	Wt::Dbo::Transaction transaction(DboSession());
	Wt::Dbo::collection<Database::Track::UIQueryResult> results = _queryModel.query();

	for (auto it = results.begin(); it != results.end(); ++it)
	{
		Database::Track::id_type id = it->get<0>();
		trackIds.push_back(id);
	}

	LMS_LOG(MOD_UI, SEV_DEBUG) << "Getting all tracks done! " << trackIds.size() << " tracks!";
}

} // namespace Desktop
} // namespace UserInterface

