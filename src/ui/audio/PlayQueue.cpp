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

#include <Wt/WItemDelegate>

#include "logger/Logger.hpp"

#include "PlayQueue.hpp"

namespace {

	void modelForceRefreshDataRow(Wt::WStandardItemModel *model, int row)
	{
		for (int i = 0; i < model->columnCount(); ++i)
			model->setData(row, i, model->data(row, i));
	}

	void swapRows(Wt::WStandardItemModel *model, int row1, int row2)
	{
		// Copy column by column
		for (int i = 0; i < model->columnCount(); ++i)
		{
			boost::any tmp = model->data(row1, i);		// 1 -> tmp
			model->setData(row1, i, model->data(row2, i));	// 2 -> 1
			model->setData(row2, i, tmp);			// tmp-> 2
		}
	}
}

namespace UserInterface {

static const int trackIdInvalid = -1;

class PlayQueueItemDelegate : public Wt::WItemDelegate
{
	public:
		PlayQueueItemDelegate(Wt::WObject *parent = 0) : Wt::WItemDelegate(parent), _selectedRowId(trackIdInvalid) {}

		void setSelectedRowId(int rowId)	{ _selectedRowId = rowId; }

		Wt::WWidget* update(Wt::WWidget *widget, const Wt::WModelIndex &index, Wt::WFlags< Wt::ViewItemRenderFlag > flags)
		{
			Wt::WWidget* res = Wt::WItemDelegate::update(widget, index, flags);

			if (res && index.isValid() && index.row() == _selectedRowId)
				res->toggleStyleClass("playqueue-playing", true);

			return res;
		}

	private:

		int _selectedRowId;
};



PlayQueue::PlayQueue(Database::Handler& db, Wt::WContainerWidget* parent)
: Wt::WTableView(parent),
_db(db),
_playedId(trackIdInvalid)
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

	_itemDelegate = new PlayQueueItemDelegate();
	this->setItemDelegate(_itemDelegate);

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
PlayQueue::play(int trackId)
{
	readTrack(trackId);
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
			_model->setData(dataRow, 2, track->getArtistName() + " - " + track->getName());
			_model->setData(dataRow, 3, track->getDuration());
		}
	}
}

void
PlayQueue::clear(void)
{
	_model->removeRows(0, _model->rowCount());

	// Reset play id
	_playedId = trackIdInvalid;
}

void
PlayQueue::handlePlaybackComplete(void)
{
	// Play the next track
	if (_playedId != trackIdInvalid)
		readTrack(_playedId + 1);
}

void
PlayQueue::playNext(void)
{
	readTrack( _playedId != trackIdInvalid ? _playedId + 1 : 0);
}

void
PlayQueue::playPrevious(void)
{
	readTrack( _playedId >= 1 ? _playedId - 1 : 0);
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
			setPlayingTrackId(rowId);

			_sigTrackPlay.emit(track->getPath());

			return;
		}

		++rowId;
	}

}

void
PlayQueue::setPlayingTrackId(int newRowId)
{
	int oldRowId = _playedId;
	_playedId = newRowId;

	_itemDelegate->setSelectedRowId(newRowId);

	// Hack re-set the data in order to trigger the rerending of the widget
	// calling the update method of our custom item delegate gives bad results
	if (oldRowId >= 0)
		modelForceRefreshDataRow(_model, oldRowId);

	if (newRowId >= 0)
		modelForceRefreshDataRow(_model, newRowId);
}

void
PlayQueue::delSelected(void)
{
	int minId = _model->rowCount();
	Wt::WModelIndexSet indexSet = this->selectedIndexes();

	BOOST_REVERSE_FOREACH(Wt::WModelIndex index, indexSet)
	{
		_model->removeRow(index.row());
		if (index.row() < minId)
			minId = index.row();
	}

	// If the current played track is removed, make sure to unselect it
	_itemDelegate->setSelectedRowId(trackIdInvalid);

	renumber(minId, _model->rowCount() - 1);
}

void
PlayQueue::moveSelectedUp(void)
{
	int minId = _model->rowCount();
	int maxId = 0;

	Wt::WModelIndexSet indexSet = this->selectedIndexes();
	Wt::WModelIndexSet newIndexSet;

	// ordered from up to down
	BOOST_FOREACH(Wt::WModelIndex index, indexSet)
	{
		// Do nothing if the first selected index is on the top
		if (index.row() == 0)
			return;

		// TODO optimize for blocks
		swapRows(_model, index.row() - 1, index.row());

		if (index.row() - 1 < minId)
			minId = index.row() - 1;

		if (index.row() > maxId)
			maxId = index.row();

		// Update playing status
		if (_playedId == index.row())
			setPlayingTrackId(_playedId - 1);
		else if (_playedId == index.row() - 1)
			setPlayingTrackId(_playedId + 1);

		newIndexSet.insert( _model->index(index.row() - 1, 0));
	}

	this->setSelectedIndexes( newIndexSet );

	renumber(minId, maxId);
}


void
PlayQueue::moveSelectedDown(void)
{
	int minId = _model->rowCount();
	int maxId = 0;

	Wt::WModelIndexSet indexSet = this->selectedIndexes();
	Wt::WModelIndexSet newIndexSet;

	// ordered from up to down
	BOOST_REVERSE_FOREACH(Wt::WModelIndex index, indexSet)
	{
		// Do nothing if the last selected index is the last one
		if (index.row() == _model->rowCount() - 1)
			return;

		// TODO optimize for blocks
		swapRows(_model, index.row(), index.row() + 1);

		if (index.row() < minId)
			minId = index.row();

		if (index.row() + 1 > maxId)
			maxId = index.row() + 1;

		// Update playing status
		if (_playedId == index.row())
			setPlayingTrackId(_playedId + 1);
		else if (_playedId == index.row() + 1)
			setPlayingTrackId(_playedId - 1);

		newIndexSet.insert( _model->index(index.row() + 1, 0));
	}

	this->setSelectedIndexes( newIndexSet );

	renumber(minId, maxId);
}

void
PlayQueue::renumber(int firstId, int lastId)
{
	for (int i = firstId; i <= lastId; ++i)
		_model->setData( i, 1, i + 1);
}



} // namespace UserInterface

