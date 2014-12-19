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

#include "database/AudioTypes.hpp"

#include "logger/Logger.hpp"

#include "TableFilter.hpp"

namespace UserInterface {

using namespace Database;

TableFilter::TableFilter(Database::Handler& db, Database::SearchFilter::Field field, std::vector<Wt::WString> columnNames, Wt::WContainerWidget* parent)
: Wt::WTableView( parent ),
Filter(),
_db(db),
_field(field)
{
	SearchFilter filter;

	switch (field)
	{
		case Database::SearchFilter::Field::Artist:
			Track::updateArtistQueryModel(_db.getSession(), _queryModel, filter, columnNames);
			break;
		case Database::SearchFilter::Field::Release:
			Track::updateReleaseQueryModel(_db.getSession(), _queryModel, filter, columnNames);
			break;
		case Database::SearchFilter::Field::Genre:
			Genre::updateGenreQueryModel(_db.getSession(), _queryModel, filter, columnNames);
			break;
		default:
			break;
	}

	this->setSelectionMode(Wt::ExtendedSelection);
	this->setSortingEnabled(true);
	this->setAlternatingRowColors(true);
	this->setModel(&_queryModel);

	this->setColumnWidth(1, 50);

	this->selectionChanged().connect(this, &TableFilter::emitUpdate);

	setLayoutSizeAware(true);

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
TableFilter::layoutSizeChanged (int width, int height)
{
	std::size_t trackColumnSize = this->columnWidth(1).toPixels();
	// Set the remaining size for the name column
	this->setColumnWidth(0, width - trackColumnSize - (7 * 2) - 2);
}

// Set constraints on this filter
void
TableFilter::refresh(SearchFilter& filter)
{
	switch (_field)
	{
		case Database::SearchFilter::Field::Artist:
			Track::updateArtistQueryModel(_db.getSession(), _queryModel, filter);
			break;
		case Database::SearchFilter::Field::Release:
			Track::updateReleaseQueryModel(_db.getSession(), _queryModel, filter);
			break;
		case Database::SearchFilter::Field::Genre:
			Genre::updateGenreQueryModel(_db.getSession(), _queryModel, filter);
			break;
		default:
			break;
	}
}

// Get constraint created by this filter
void
TableFilter::getConstraint(SearchFilter& filter)
{
	Wt::WModelIndexSet indexSet = this->selectedIndexes();

	BOOST_FOREACH(Wt::WModelIndex index, indexSet) {

		if (!index.isValid())
			continue;

		const ResultType& result = _queryModel.resultRow( index.row() );

		std::string     name = result.get<0>();

		filter.exactMatch[_field].push_back(name);
	}
}

} // namespace UserInterface

