#include <boost/foreach.hpp>

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

	std::cout << _table << ", generated query = '" << sqlQuery.get() + " UNION " + AllSqlQuery.get() << "'" << std::endl;

	Wt::Dbo::Query<ResultType> query = _db.getSession().query<ResultType>( sqlQuery.get() + " UNION " + AllSqlQuery.get() );

	query.orderBy("ORDERBY DESC," + _table + "." + _field);

	BOOST_FOREACH(const std::string& bindArg, sqlQuery.where().getBindArgs()) {
		std::cout << "Binding value '" << bindArg << "'" << std::endl;
		query.bind(bindArg);
	}

	_queryModel.setQuery( query, true /* Keep columns */);

	std::cout << "Finish !" << std::endl;
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

