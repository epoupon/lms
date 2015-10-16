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

#include <Wt/WItemDelegate>

#include "database/Types.hpp"
#include "logger/Logger.hpp"

#include "LmsApplication.hpp"
#include "TableFilter.hpp"

namespace UserInterface {
namespace Desktop {

using namespace Database;

TableFilterGenre::TableFilterGenre(Wt::WContainerWidget* parent)
: Wt::WTableView( parent ), Filter()
{
	const std::vector<Wt::WString> columnNames = {"Genre", "Tracks"};

	SearchFilter filter;

	Genre::updateUIQueryModel(DboSession(), _queryModel, filter, columnNames);

	this->setSelectionMode(Wt::ExtendedSelection);
	this->setSortingEnabled(true);
	this->setAlternatingRowColors(true);
	this->setModel(&_queryModel);

	this->setColumnWidth(1, 80);

	this->selectionChanged().connect(this, &TableFilterGenre::emitUpdate);

#if WT_VERSION >= 0X03030400
	this->setOverflow(Wt::WContainerWidget::OverflowHidden, Wt::Horizontal);
	this->setOverflow(Wt::WContainerWidget::OverflowScroll, Wt::Vertical);
#endif

	this->setLayoutSizeAware(true);

	_queryModel.setBatchSize(100);

	// If an item is double clicked, select and emit signal
	this->doubleClicked().connect( std::bind([=] (Wt::WModelIndex idx, Wt::WMouseEvent evt)
	{
		if (!idx.isValid())
			return;

		Wt::WModelIndexSet indexSet;
		indexSet.insert(idx);

		this->setSelectedIndexes( indexSet );
		_sigDoubleClicked.emit( );

	}, std::placeholders::_1, std::placeholders::_2));

}

void
TableFilterGenre::layoutSizeChanged (int width, int height)
{
	std::size_t trackColumnSize = this->columnWidth(1).toPixels();
	// Set the remaining size for the name column
	this->setColumnWidth(0, width - trackColumnSize - (7 * 2) - 2);
}

// Set constraints on this filter
void
TableFilterGenre::refresh(SearchFilter& filter)
{
	Genre::updateUIQueryModel(DboSession(), _queryModel, filter);
}

// Get constraint created by this filter
void
TableFilterGenre::getConstraint(SearchFilter& filter)
{
	Wt::WModelIndexSet indexSet = this->selectedIndexes();

	for (Wt::WModelIndex index : indexSet)
	{
		if (!index.isValid())
			continue;

		// TODO set invisible track id
		Database::Genre::id_type id = _queryModel.resultRow( index.row() ).get<0>();

		filter.idMatch[Database::SearchFilter::Field::Genre].push_back(id);
	}
}

TableFilterArtist::TableFilterArtist(Wt::WContainerWidget* parent)
: Wt::WTableView( parent ), Filter()
{
	const std::vector<Wt::WString> columnNames = {"Artist", "Releases", "Tracks"};

	SearchFilter filter;

	Artist::updateUIQueryModel(DboSession(), _queryModel, filter, columnNames);

	this->setSelectionMode(Wt::ExtendedSelection);
	this->setSortingEnabled(true);
	this->setAlternatingRowColors(true);
	this->setModel(&_queryModel);

	this->setColumnWidth(1, 80);
	this->setColumnWidth(2, 80);

	this->selectionChanged().connect(this, &TableFilterArtist::emitUpdate);

#if WT_VERSION >= 0X03030400
	this->setOverflow(Wt::WContainerWidget::OverflowHidden, Wt::Horizontal);
	this->setOverflow(Wt::WContainerWidget::OverflowScroll, Wt::Vertical);
#endif

	this->setLayoutSizeAware(true);

	_queryModel.setBatchSize(100);

	// If an item is double clicked, select and emit signal
	this->doubleClicked().connect( std::bind([=] (Wt::WModelIndex idx, Wt::WMouseEvent evt)
	{
		if (!idx.isValid())
			return;

		Wt::WModelIndexSet indexSet;
		indexSet.insert(idx);

		this->setSelectedIndexes( indexSet );
		_sigDoubleClicked.emit( );

	}, std::placeholders::_1, std::placeholders::_2));

}

void
TableFilterArtist::layoutSizeChanged (int width, int height)
{
	std::size_t otherColumnsSize = this->columnWidth(1).toPixels() + this->columnWidth(2).toPixels();
	// Set the remaining size for the name column
	this->setColumnWidth(0, width - otherColumnsSize - (7 * 3) - 2);
}

// Set constraints on this filter
void
TableFilterArtist::refresh(SearchFilter& filter)
{
	Artist::updateUIQueryModel(DboSession(), _queryModel, filter);
}

// Get constraint created by this filter
void
TableFilterArtist::getConstraint(SearchFilter& filter)
{
	Wt::WModelIndexSet indexSet = this->selectedIndexes();

	for (Wt::WModelIndex index : indexSet)
	{
		if (!index.isValid())
			continue;

		Artist::id_type id = _queryModel.resultRow( index.row() ).get<0>();

		filter.idMatch[SearchFilter::Field::Artist].push_back(id);
	}
}

TableFilterRelease::TableFilterRelease(Wt::WContainerWidget* parent)
: Wt::WTableView( parent ), Filter()
{
	const std::vector<Wt::WString> columnNames = {"Release", "Date", "Tracks"};

	SearchFilter filter;

	Release::updateUIQueryModel(DboSession(), _queryModel, filter, columnNames);

	this->setSelectionMode(Wt::ExtendedSelection);
	this->setSortingEnabled(true);
	this->setAlternatingRowColors(true);
	this->setModel(&_queryModel);

	this->setColumnWidth(1, 60);
	this->setColumnWidth(2, 80);

	// Date display, just the year
	{
		Wt::WItemDelegate *delegate = new Wt::WItemDelegate(this);
		delegate->setTextFormat("yyyy");
		this->setItemDelegateForColumn(1, delegate);
	}

	this->selectionChanged().connect(this, &TableFilterRelease::emitUpdate);

#if WT_VERSION >= 0X03030400
	this->setOverflow(Wt::WContainerWidget::OverflowHidden, Wt::Horizontal);
	this->setOverflow(Wt::WContainerWidget::OverflowScroll, Wt::Vertical);
#endif

	this->setLayoutSizeAware(true);

	_queryModel.setBatchSize(100);

	// If an item is double clicked, select and emit signal
	this->doubleClicked().connect( std::bind([=] (Wt::WModelIndex idx, Wt::WMouseEvent evt)
	{
		if (!idx.isValid())
			return;

		Wt::WModelIndexSet indexSet;
		indexSet.insert(idx);

		this->setSelectedIndexes( indexSet );
		_sigDoubleClicked.emit( );

	}, std::placeholders::_1, std::placeholders::_2));

}

void
TableFilterRelease::layoutSizeChanged (int width, int height)
{
	std::size_t otherColumnSizes = this->columnWidth(1).toPixels() + this->columnWidth(2).toPixels();
	// Set the remaining size for the name column
	this->setColumnWidth(0, width - otherColumnSizes - (7 * 3) - 2);
}

// Set constraints on this filter
void
TableFilterRelease::refresh(SearchFilter& filter)
{
	Release::updateUIQueryModel(DboSession(), _queryModel, filter);
}

// Get constraint created by this filter
void
TableFilterRelease::getConstraint(SearchFilter& filter)
{
	Wt::WModelIndexSet indexSet = this->selectedIndexes();

	for (Wt::WModelIndex index : indexSet)
	{
		if (!index.isValid())
			continue;

		Release::id_type id = _queryModel.resultRow( index.row() ).get<0>();

		filter.idMatch[Database::SearchFilter::Field::Release].push_back(id);
	}
}

} // namespace Desktop
} // namespace UserInterface

