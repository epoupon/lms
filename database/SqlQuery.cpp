#include <algorithm>

#include <boost/foreach.hpp>

#include <sstream>
#include <stdexcept>

#include "SqlQuery.hpp"

WhereClause&
WhereClause::And(const WhereClause& otherClause)
{
	if (!otherClause._clause.empty()) {
		if (!_clause.empty())
			_clause += " AND ";
		_clause += "(" + otherClause._clause + ")";

		// Add associated bind args
		BOOST_FOREACH(const std::string& otherBindArg, otherClause._bindArgs) {
			_bindArgs.push_back(otherBindArg);
		}
	}
	return *this;
}

WhereClause&
WhereClause::Or(const WhereClause& otherClause)
{
	if (!otherClause._clause.empty()) {
		if (!_clause.empty())
			_clause += " OR ";
		_clause += "(" + otherClause._clause + ")";

		// Add associated bind args
		BOOST_FOREACH(const std::string& otherBindArg, otherClause._bindArgs) {
			_bindArgs.push_back(otherBindArg);
		}
	}
	return *this;
}

std::string
WhereClause::get(void) const
{
	if (!_clause.empty())
		return "WHERE " + _clause;
	else
		return "";
}

WhereClause&
WhereClause::bind(const std::string& bindArg)
{
	if (_bindArgs.size() >= static_cast<std::size_t>( std::count(_clause.begin(), _clause.end(), '?') ))
		throw std::runtime_error("Too many bind args!");

	_bindArgs.push_back(bindArg);

	return *this;
}


SelectStatement&
SelectStatement::And(const SelectStatement& statement)
{
	if( _statement.empty() && !statement._statement.empty())
		_statement = "SELECT ";
	else if (!_statement.empty() && !statement._statement.empty())
		_statement += ",";

	_statement += statement._statement;
	return *this;
}

FromClause::FromClause(const std::string& clause)
{
	_clause.push_back(clause);
}


GroupByStatement&
GroupByStatement::And(const GroupByStatement& statement)
{
	if( _statement.empty() && !statement._statement.empty())
		_statement = "GROUP BY ";
	else if (!_statement.empty() && !statement._statement.empty())
		_statement += ",";

	_statement += statement._statement;
	return *this;
}


FromClause&
FromClause::And(const FromClause& clause)
{
	BOOST_FOREACH(const std::string fromClause, clause._clause) {
		_clause.push_back(fromClause);
	}

	_clause.sort();
	_clause.unique();

	return *this;
}

std::string
FromClause::get() const
{
	std::ostringstream oss;

	if (!_clause.empty())
	{
		oss << "FROM ";
		for (std::list<std::string>::const_iterator it = _clause.begin(); it != _clause.end(); ++it) {
			if (it != _clause.begin())
				oss << ",";

			oss << *it;
		}
	}

	return oss.str();
}

std::string
SqlQuery::get(void) const
{
	std::ostringstream oss;

	oss << _selectStatement.get();

	if (!_fromClause.get().empty())
		oss << " " << _fromClause.get();

	if (!_whereClause.get().empty())
		oss << " " << _whereClause.get();

	if (!_groupByStatement.get().empty())
		oss << " " << _groupByStatement.get();

	return oss.str();
}

