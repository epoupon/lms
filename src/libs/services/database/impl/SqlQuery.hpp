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

#pragma once

#include <list>
#include <string>


class WhereClause
{
	public:
		WhereClause() {}
		WhereClause(const std::string& clause) { _clause = clause; }

		WhereClause& And(const WhereClause& clause);
		WhereClause& Or(const WhereClause& clause);

		// Arguments binding (for each '?' in where clause)
		WhereClause& bind(const std::string& arg);

		std::string get() const;
		const std::list<std::string>&	getBindArgs() const	{return _bindArgs;}

	private:
		std::string _clause;		// WHERE clause
		std::list<std::string>  _bindArgs;
};

class InnerJoinClause
{
	public:
		InnerJoinClause() {}
		InnerJoinClause(const std::string& clause);

		InnerJoinClause&	And(const InnerJoinClause& clause);
		std::string get() const	{ return _clause;}

	private:
		std::string _clause;
};

class GroupByStatement
{
	public:
		GroupByStatement() {}
		GroupByStatement(const std::string& statement) { _statement = statement; }

		GroupByStatement& And(const GroupByStatement& statement);

		std::string get() const      {return _statement;}

	private:
		std::string _statement;		// SELECT statement
};

class SelectStatement
{
	public:
		SelectStatement() {};
		SelectStatement(const std::string& item);

		SelectStatement& And(const std::string& item);

		std::string get() const;

	private:
		std::list<std::string>	_statement;
};

class FromClause
{
	public:
		FromClause() {}
		FromClause(const std::string& clause);

		FromClause& And(const FromClause& clause);

		std::string get() const;

	private:
		std::list<std::string>	_clause;
};

class SqlQuery
{
	public:
		SelectStatement&	select()		{ return _selectStatement;}
		SelectStatement&	select(const std::string& statement)		{ _selectStatement = SelectStatement(statement); return _selectStatement; }
		FromClause&		from()		{ return _fromClause; }
		FromClause&		from(const std::string& clause)			{ _whereClause = WhereClause(clause); return _fromClause; }
		InnerJoinClause&	innerJoin()		{ return _innerJoinClause; }
		WhereClause&		where()		{ return _whereClause; }
		const WhereClause&	where() const	{ return _whereClause; }
		GroupByStatement&	groupBy() 		{ return _groupByStatement; }
		const GroupByStatement&	groupBy() const	{ return _groupByStatement; }

		std::string get() const;

	private:
		SelectStatement		_selectStatement;	// SELECT statement
		InnerJoinClause		_innerJoinClause;	// INNER JOIN
		FromClause		_fromClause;		// FROM tables
		WhereClause		_whereClause;		// WHERE clause
		GroupByStatement	_groupByStatement;	// GROUP BY statement
};

