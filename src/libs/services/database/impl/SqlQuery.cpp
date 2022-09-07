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

#include "SqlQuery.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

WhereClause&
WhereClause::And(const WhereClause& otherClause)
{
	if (!otherClause._clause.empty()) {
		if (!_clause.empty())
			_clause += " AND ";
		_clause += "(" + otherClause._clause + ")";

		// Add associated bind args
		for (const std::string& otherBindArg : otherClause._bindArgs)
		{
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
		for (const std::string& otherBindArg : otherClause._bindArgs)
		{
			_bindArgs.push_back(otherBindArg);
		}
	}
	return *this;
}

std::string
WhereClause::get() const
{
	if (!_clause.empty())
		return "WHERE " + _clause;
	else
		return "";
}

WhereClause&
WhereClause::bind(const std::string& bindArg)
{
	assert(_bindArgs.size() < static_cast<std::size_t>(std::count(_clause.begin(), _clause.end(), '?')));

	_bindArgs.push_back(bindArg);

	return *this;
}

InnerJoinClause::InnerJoinClause(const std::string& clause)
:_clause(clause)
{
}

InnerJoinClause&
InnerJoinClause::And(const InnerJoinClause& clause)
{
	if (!_clause.empty())
		_clause += " ";

	_clause += "INNER JOIN " + clause._clause;

	return *this;
}

SelectStatement::SelectStatement(const std::string& statement)
{
	And(statement);
}

SelectStatement&
SelectStatement::And(const std::string& statement)
{
	_statement.push_back(statement);

	_statement.sort();
	_statement.unique();

	return *this;
}

std::string
SelectStatement::get() const
{
	std::string res = "SELECT ";

	for (std::list<std::string>::const_iterator it = _statement.begin(); it != _statement.end(); ++it)
	{
		if (it != _statement.begin())
			res += ",";
		res += *it;
	}

	return res;
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

FromClause::FromClause(const std::string& clause)
{
	_clause.push_back(clause);
}

FromClause&
FromClause::And(const FromClause& clause)
{
	for (const std::string& fromClause : clause._clause)
	{
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
SqlQuery::get() const
{
	std::ostringstream oss;

	oss << _selectStatement.get();

	if (!_fromClause.get().empty())
		oss << " " << _fromClause.get();

	if (!_innerJoinClause.get().empty())
		oss << " " << _innerJoinClause.get();

	if (!_whereClause.get().empty())
		oss << " " << _whereClause.get();

	if (!_groupByStatement.get().empty())
		oss << " " << _groupByStatement.get();

	return oss.str();
}

