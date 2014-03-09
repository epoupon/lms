#ifndef SQL_QUERY_HPP___
#define SQL_QUERY_HPP___

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
		const std::list<std::string>&	getBindArgs(void) const	{return _bindArgs;}

	private:

		std::string _clause;		// WHERE clause
		std::list<std::string>  _bindArgs;

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
		SelectStatement() {}
		SelectStatement(const std::string& statement) { _statement = statement; }

		SelectStatement& And(const SelectStatement& statement);

		std::string get() const      {return _statement;}

	private:

		std::string _statement;		// SELECT statement
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

		SelectStatement&	select(void)		{ return _selectStatement;}
		FromClause&		from(void)		{ return _fromClause; }
		WhereClause&		where(void)		{ return _whereClause; }
		const WhereClause&	where(void) const	{ return _whereClause; }
		GroupByStatement&	groupBy(void) 		{ return _groupByStatement; }
		const GroupByStatement&	groupBy(void) const	{ return _groupByStatement; }

		std::string get(void) const;

	private:

		SelectStatement		_selectStatement;	// SELECT statement
		FromClause		_fromClause;		// FROM tables
		WhereClause		_whereClause;		// WHERE clause
		GroupByStatement	_groupByStatement;	// GROUP BY statement
};

#endif

