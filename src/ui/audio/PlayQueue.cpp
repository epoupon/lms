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

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

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

static const int trackPosInvalid = -1;

class TrackSelector
{
	public:
		TrackSelector();

		void setShuffle(bool enable);
		void setLoop(bool enable) { _loop = enable; }

		int getPrevious(void);
		int getNext(void);
		int getCurrent(void);

		// Set the internal pos thanks to the track pos
		void setPosByRowId(int rowId);
		void setPos(int pos)	{ _curPos = pos; }

		void setSize(std::size_t size);
		std::size_t getSize(void) const { return _size;}

	private:

		void refreshPositions();

		bool _loop;
		bool _shuffle;

		std::size_t _size;
		std::size_t _curPos;

		std::vector<int> _trackPos;
};

TrackSelector::TrackSelector()
: _loop(false),
_shuffle(false),
_curPos(0)
{
}

	void
TrackSelector::refreshPositions()
{
	_trackPos.clear();

	if (_size == 0)
		return;

	for (std::size_t i = 0; i < _size; ++i)
		_trackPos.push_back(i);

	// Now shuffle
	// Perform size/2 permutations
	boost::random::mt19937 rng;         // produces randomness out of thin air
	rng.seed(static_cast<unsigned int>(std::time(0)));

	for (std::size_t i = 0; i < _size; ++i)
	{
		boost::random::uniform_int_distribution<> distribution(i, _size - 1);
		int val1 = distribution(rng);
		int tmp = _trackPos[i];
		_trackPos[i] = _trackPos[val1];
		_trackPos[val1] = tmp;
	}
}

int
TrackSelector::getCurrent()
{
	return _shuffle ? _trackPos[_curPos] : _curPos;
}

void
TrackSelector::setShuffle(bool enable)
{
	_shuffle = enable;

	if (_shuffle)
		setPosByRowId(_curPos);
	else
		setPos(_trackPos[_curPos]);
}

int
TrackSelector::getNext()
{
	if (_size == 0)
		return trackPosInvalid;

	if (_curPos == _size - 1)
	{
		if (!_loop)
			return trackPosInvalid;
		else
			_curPos = 0;
	}
	else
		++_curPos;

	return _shuffle ? _trackPos[_curPos] : _curPos;
}

int
TrackSelector::getPrevious()
{
	if (_size == 0)
		return trackPosInvalid;

	if (_curPos == 0)
	{
		if (!_loop)
			return trackPosInvalid;
		else
			_curPos = _size - 1;
	}
	else
		--_curPos;

	return _shuffle ? _trackPos[_curPos] : _curPos;
}

void
TrackSelector::setPosByRowId(int rowId)
{
	if (_shuffle)
	{
		for (std::size_t i = 0; i < _trackPos.size(); ++i)
		{
			if (rowId == _trackPos[i])
			{
				_curPos = i;
				break;
			}
		}
	}
	else
		_curPos = rowId;
}

void
TrackSelector::setSize(std::size_t size)
{
	_size = size;
	_curPos = 0;
	refreshPositions();
}

class PlayQueueItemDelegate : public Wt::WItemDelegate
{
	public:
		PlayQueueItemDelegate(Wt::WObject *parent = 0) : Wt::WItemDelegate(parent), _selectedRowPos(trackPosInvalid) {}

		void setSelectedRowPos(int rowPos)	{ _selectedRowPos = rowPos; }

		Wt::WWidget* update(Wt::WWidget *widget, const Wt::WModelIndex &index, Wt::WFlags< Wt::ViewItemRenderFlag > flags)
		{
			Wt::WWidget* res = Wt::WItemDelegate::update(widget, index, flags);

			if (res && index.isValid())
			{
				if (_selectedRowPos != trackPosInvalid && index.row() == _selectedRowPos)
					res->toggleStyleClass("playqueue-playing", true);
				else
					res->toggleStyleClass("playqueue-playing", false);
			}

			return res;
		}

	private:

		int _selectedRowPos;
};



PlayQueue::PlayQueue(Database::Handler& db, Wt::WContainerWidget* parent)
: Wt::WTableView(parent),
_db(db),
_curPlayedTrackPos(trackPosInvalid),
_trackSelector(new TrackSelector())
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
		play(idx.row());

	}, std::placeholders::_1, std::placeholders::_2));

}

void
PlayQueue::setShuffle(bool enable)
{
	_trackSelector->setShuffle(enable);
}

void
PlayQueue::setLoop(bool enable)
{
	_trackSelector->setLoop(enable);
}

void
PlayQueue::play()
{
	_trackSelector->setPos(0);

	if (!readTrack(_trackSelector->getCurrent()))
		playNext();
}

void
PlayQueue::play(int rowId)
{
	// Update the track selector to use the requested track as current position
	_trackSelector->setPosByRowId(rowId);

	if (!readTrack(_trackSelector->getCurrent()))
		playNext();
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

	_trackSelector->setSize( _model->rowCount() );
}

void
PlayQueue::clear(void)
{
	_model->removeRows(0, _model->rowCount());

	// Reset play id
	_curPlayedTrackPos = trackPosInvalid;
	_trackSelector->setSize( 0 );
}

void
PlayQueue::handlePlaybackComplete(void)
{
	playNext();
}

void
PlayQueue::playNext(void)
{
	std::size_t nbTries = _trackSelector->getSize();

	while (nbTries > 0)
	{
		int pos = _trackSelector->getNext();
		if (pos == trackPosInvalid)
			break;

		if (readTrack(pos))
			break;

		--nbTries;
	}
}

void
PlayQueue::playPrevious(void)
{
	std::size_t nbTries = _trackSelector->getSize();

	while (nbTries > 0)
	{
		int pos = _trackSelector->getPrevious();
		if (pos == trackPosInvalid)
			break;

		if (readTrack(pos))
			break;

		--nbTries;
	}
}

bool
PlayQueue::readTrack(int rowPos)
{
	Wt::Dbo::Transaction transaction(_db.getSession());

	LMS_LOG(MOD_UI, SEV_DEBUG) << "Reading track at pos " << rowPos;
	Database::Track::id_type trackId = boost::any_cast<Database::Track::id_type>(_model->data(rowPos, 0));

	Database::Track::pointer track = Database::Track::getById(_db.getSession(), trackId);
	if (track)
	{
		setPlayingTrackPos(rowPos);

		_sigTrackPlay.emit(track->getPath());

		return true;
	}
	else
		return false;
}

void
PlayQueue::setPlayingTrackPos(int newRowPos)
{
	int oldRowPos = _curPlayedTrackPos;
	_curPlayedTrackPos = newRowPos;

	_itemDelegate->setSelectedRowPos(newRowPos);

	// Hack re-set the data in order to trigger the rerending of the widget
	// calling the update method of our custom item delegate gives bad results
	if (oldRowPos >= 0)
		modelForceRefreshDataRow(_model, oldRowPos);

	if (newRowPos >= 0)
		modelForceRefreshDataRow(_model, newRowPos);
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
	_itemDelegate->setSelectedRowPos(trackPosInvalid);
	_trackSelector->setSize(_model->rowCount());

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
		if (_curPlayedTrackPos == index.row())
			setPlayingTrackPos(_curPlayedTrackPos - 1);
		else if (_curPlayedTrackPos == index.row() - 1)
			setPlayingTrackPos(_curPlayedTrackPos + 1);

		newIndexSet.insert( _model->index(index.row() - 1, 0));
	}

	_trackSelector->setPosByRowId(_curPlayedTrackPos);
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
		if (_curPlayedTrackPos == index.row())
			setPlayingTrackPos(_curPlayedTrackPos + 1);
		else if (_curPlayedTrackPos == index.row() + 1)
			setPlayingTrackPos(_curPlayedTrackPos - 1);

		newIndexSet.insert( _model->index(index.row() + 1, 0));
	}

	_trackSelector->setPosByRowId(_curPlayedTrackPos);
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

