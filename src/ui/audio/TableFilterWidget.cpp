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

#include "TableFilterWidget.hpp"

namespace UserInterface {

TableFilterWidget::TableFilterWidget(Database::Handler& db, std::string table, std::string field, Wt::WContainerWidget* parent)
: FilterWidget( parent ),
_db(db),
_table(table),
_field(field),
_tableView(nullptr)
{
	_queryModel.setQuery( _db.getSession().query< ResultType >("select " + _table + "." + _field + ",count(DISTINCT track.id),0 as ORDERBY from track,artist,release,genre,track_genre WHERE track.artist_id = artist.id and track.release_id = release.id and track_genre.track_id = track.id and genre.id = track_genre.genre_id GROUP BY " + _table + "." + _field + " UNION select '<All>',0,1 AS ORDERBY").orderBy("ORDERBY DESC," + _table + "." + _field));
	_queryModel.addColumn( _table + "." + _field, table);
	_queryModel.addColumn( "count(DISTINCT track.id)", "Tracks");

	_tableView = new Wt::WTableView( this );
	_tableView->resize(250, 200);	// TODO
	_tableView->setSelectionMode(Wt::ExtendedSelection);
	_tableView->setSortingEnabled(false);
	_tableView->setAlternatingRowColors(true);
	_tableView->setModel(&_queryModel);

	_tableView->selectionChanged().connect(this, &TableFilterWidget::emitUpdate);

	_queryModel.setBatchSize(100);
}

// Set constraints on this filter
void
TableFilterWidget::refresh(const Constraint& constraint)
{
	SqlQuery sqlQuery;

	sqlQuery.select(_table + "." + _field + ",count(DISTINCT track.id),0 as ORDERBY");
	sqlQuery.from().And( FromClause("artist,release,track,genre,track_genre")) ;
	sqlQuery.where().And(constraint.where);	// Add constraint made by other filters
	sqlQuery.groupBy().And( _table + "." + _field);	// Add constraint made by other filters

	SqlQuery AllSqlQuery;
	AllSqlQuery.select("'<All>',0,1 AS ORDERBY");

	LMS_LOG(MOD_UI, SEV_DEBUG) << _table << ", generated query = '" << sqlQuery.get() + " UNION " + AllSqlQuery.get() << "'";

	Wt::Dbo::Query<ResultType> query = _db.getSession().query<ResultType>( sqlQuery.get() + " UNION " + AllSqlQuery.get() );

	query.orderBy("ORDERBY DESC," + _table + "." + _field);

	BOOST_FOREACH(const std::string& bindArg, sqlQuery.where().getBindArgs()) {
		LMS_LOG(MOD_UI, SEV_DEBUG) << "Binding value '" << bindArg << "'";
		query.bind(bindArg);
	}

	_queryModel.setQuery( query, true /* Keep columns */);

	LMS_LOG(MOD_UI, SEV_DEBUG) << "Finish !";
}

// Get constraint created by this filter
void
TableFilterWidget::getConstraint(Constraint& constraint)
{
	Wt::WModelIndexSet indexSet = _tableView->selectedIndexes();

	// WHERE statement
	WhereClause clause;

	BOOST_FOREACH(Wt::WModelIndex index, indexSet) {

		if (!index.isValid())
			continue;

		ResultType result = _queryModel.resultRow( index.row() );

		// Get the track part
		std::string	name( result.get<0>() );
		bool		isAll( result.get<2>() ) ;

		if (isAll) {
			// no constraint, just return
			return;
		}

		clause.Or(_table + "." + _field + " = ?").bind(name);
	}

	// Adding our WHERE clause
	constraint.where.And( clause );
}

} // namespace UserInterface

