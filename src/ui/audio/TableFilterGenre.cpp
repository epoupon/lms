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

#include "TableFilterGenre.hpp"

namespace UserInterface {

TableFilterGenre::TableFilterGenre(Database::Handler& db, Wt::WContainerWidget* parent)
: Wt::WTableView( parent ),
Filter(),
_db(db)
{
	_queryModel.setQuery( _db.getSession().query< ResultType >("select genre.name, COUNT(DISTINCT track.id) from genre INNER JOIN track_genre ON track_genre.genre_id = genre.id INNER JOIN track ON track.id = track_genre.track_id").orderBy("genre.name").groupBy("genre.name").orderBy("genre.name"));
	_queryModel.addColumn("genre.name", "Genre");
	_queryModel.addColumn("COUNT(DISTINCT track.id)", "tracks");

	this->setSelectionMode(Wt::ExtendedSelection);
	this->setSortingEnabled(false);
	this->setAlternatingRowColors(true);
	this->setModel(&_queryModel);

	this->selectionChanged().connect(this, &TableFilterGenre::emitUpdate);

	setLayoutSizeAware(true);

	_queryModel.setBatchSize(100);
}

void
TableFilterGenre::layoutSizeChanged (int width, int height)
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
TableFilterGenre::refresh(const Constraint& constraint)
{
	SqlQuery sqlQuery;

	sqlQuery.select("genre.name, COUNT(DISTINCT track.id)");
	sqlQuery.from().And( std::string("genre INNER JOIN track_genre ON track_genre.genre_id = genre.id INNER JOIN track ON track.id = track_genre.track_id") );
	sqlQuery.where().And(constraint.where);	// Add constraint made by other filters
	sqlQuery.groupBy().And( std::string("genre.name") );

	LMS_LOG(MOD_UI, SEV_DEBUG) << "genre, generated query = '" << sqlQuery.get() << "'";

	Wt::Dbo::Query<ResultType> query = _db.getSession().query<ResultType>( sqlQuery.get() );

	query.orderBy("genre.name");

	BOOST_FOREACH(const std::string& bindArg, sqlQuery.where().getBindArgs()) {
		LMS_LOG(MOD_UI, SEV_DEBUG) << "Binding value '" << bindArg << "'";
		query.bind(bindArg);
	}

	_queryModel.setQuery( query, true );

}

// Get constraint created by this filter
void
TableFilterGenre::getConstraint(Constraint& constraint)
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

		if (name == "<None>")
			clause.Or( std::string("track.genre_list = ?") ).bind("");
		else
			clause.Or( std::string("track.genre_list LIKE ?") ).bind("%%" + name + "%%");
	}

	// Adding our WHERE clause
	constraint.where.And( clause );

}

} // namespace UserInterface

