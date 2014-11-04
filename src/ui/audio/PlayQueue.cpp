/*
 * Copyright (C) 2014 Emeric Poupon
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

#include "PlayQueue.hpp"

namespace UserInterface {


PlayQueue::PlayQueue(Database::Handler& db, Wt::WContainerWidget* parent)
: Wt::WTableView(parent),
_db(db),
_playingTrackId(0)
{
	_model = new Wt::WStandardItemModel(0, 4, this);

	// 0 Column is hidden (track id)
	_model->setHeaderData(0, Wt::WString("#"));
	_model->setHeaderData(1, Wt::WString("#"));
	_model->setHeaderData(2, Wt::WString("Track"));
	_model->setHeaderData(3, Wt::WString("Duration"));

	this->setModel(_model);
	this->setSelectionMode(Wt::ExtendedSelection);
	this->setSortingEnabled(false);
	this->setAlternatingRowColors(true);

	this->setColumnWidth(0, 50);
	this->setColumnWidth(1, 50);

	this->setColumnHidden(0, true);

	this->doubleClicked().connect( std::bind([=] (Wt::WModelIndex idx, Wt::WMouseEvent evt)
	{
		if (!idx.isValid())
			return;

		// Update selection
		Wt::WModelIndexSet indexSet;
		indexSet.insert(idx);

		this->setSelectedIndexes( indexSet );

		// Read the requested track
		readTrack(idx.row());

	}, std::placeholders::_1, std::placeholders::_2));

}

void
PlayQueue::play(void)
{
	readTrack(0);
}

void
PlayQueue::addTracks(const std::vector<Database::Track::id_type>& trackIds)
{
	// Add tracks to model
	Wt::Dbo::Transaction transaction(_db.getSession());

	BOOST_FOREACH(Database::Track::id_type trackId, trackIds)
	{
		Database::Track::pointer track (Database::Track::getById(_db.getSession(), trackId));

		if (track)
		{
			int dataRow = _model->rowCount();

			_model->insertRows(dataRow, 1);

			_model->setData(dataRow, 0, track.id());
			_model->setData(dataRow, 1, dataRow + 1);
			_model->setData(dataRow, 2, track->getName());
			_model->setData(dataRow, 3, track->getDuration());
		}
	}
}

void
PlayQueue::clear(void)
{
	_model->removeRows(0, _model->rowCount());
}

void
PlayQueue::handlePlaybackComplete(void)
{
	// Play the next track
	readTrack(_playingTrackId + 1);
}

void
PlayQueue::readTrack(int rowId)
{
	Wt::Dbo::Transaction transaction(_db.getSession());

	while (rowId < _model->rowCount())
	{
		Database::Track::id_type trackId = boost::any_cast<Database::Track::id_type>(_model->data(rowId, 0));

		Database::Track::pointer track = Database::Track::getById(_db.getSession(), trackId);
		if (track)
		{
			_playingTrackId = rowId;
			_sigTrackPlay.emit(track->getPath());
			break;
		}

		++rowId;
	}
}


} // namespace UserInterface

