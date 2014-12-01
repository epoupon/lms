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

#include "logger/Logger.hpp"

#include "TableFilter.hpp"

namespace UserInterface {

TableFilter::TableFilter(Database::Handler& db, std::string table, std::string field, const Wt::WString& displayName, Wt::WContainerWidget* parent)
: Wt::WTableView( parent ),
Filter(),
_db(db),
_table(table),
_field(field)
{
	_queryModel.setQuery( _db.getSession().query< ResultType >("select track." + _field + ", COUNT(DISTINCT track.id) from track GROUP BY track." + _field).orderBy("track." + _field));
	_queryModel.addColumn( "track." + _field, displayName);
	_queryModel.addColumn( "COUNT(DISTINCT track.id)", "Tracks");

	this->setSelectionMode(Wt::ExtendedSelection);
	this->setSortingEnabled(false);
	this->setAlternatingRowColors(true);
	this->setModel(&_queryModel);

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
	LMS_LOG(MOD_UI, SEV_DEBUG) << "LAYOUT CHANGED!";

	/* TODO
	std::size_t trackColumnSize = this->columnWidth(1).toPixels() + 30 ;
	// Set the remaining size for the name column
	this->setColumnWidth(0, width - 7 - trackColumnSize);
	*/

}

// Set constraints on this filter
void
TableFilter::refresh(const Constraint& constraint)
{
	SqlQuery sqlQuery;

	sqlQuery.select("track." + _field + ", COUNT(DISTINCT track.id)");
	sqlQuery.from().And( FromClause("track")) ;
	sqlQuery.where().And(constraint.where);	// Add constraint made by other filters
	sqlQuery.groupBy().And( "track." + _field);	// Add constraint made by other filters

	LMS_LOG(MOD_UI, SEV_DEBUG) << _table << ", generated query = '" << sqlQuery.get() << "'";

	Wt::Dbo::Query<ResultType> query = _db.getSession().query<ResultType>( sqlQuery.get() );

	query.orderBy(_table + "." + _field);

	BOOST_FOREACH(const std::string& bindArg, sqlQuery.where().getBindArgs()) {
		LMS_LOG(MOD_UI, SEV_DEBUG) << "Binding value '" << bindArg << "'";
		query.bind(bindArg);
	}

	_queryModel.setQuery( query, true );

	LMS_LOG(MOD_UI, SEV_DEBUG) << "Finish !";
}

// Get constraint created by this filter
void
TableFilter::getConstraint(Constraint& constraint)
{
	Wt::WModelIndexSet indexSet = this->selectedIndexes();

	// WHERE statement
	WhereClause clause;

	BOOST_FOREACH(Wt::WModelIndex index, indexSet) {

		if (!index.isValid())
			continue;

		const ResultType& result = _queryModel.resultRow( index.row() );

		// Get the track part
		std::string	name(result.get<0>());

		clause.Or(_table + "." + _field + " = ?").bind(name);
	}

	// Adding our WHERE clause
	constraint.where.And( clause );
}

} // namespace UserInterface

