#ifndef TABLE_VIEW_FILTER_WIDGET_HPP
#define TABLE_VIEW_FILTER_WIDGET_HPP


#include <Wt/Dbo/QueryModel>
#include <Wt/WTableView>

#include "FilterWidget.hpp"
#include "database/DatabaseHandler.hpp"

namespace UserInterface {

class TableFilterWidget : public FilterWidget
{

	public:
		TableFilterWidget(Database::Handler& db, std::string table, std::string field, Wt::WContainerWidget* parent = 0);

		// Set constraints on this filter
		virtual void refresh(const Constraint& constraint);

		// Get constraints created by this filter
		virtual void getConstraint(Constraint& constraint);

	protected:

		Database::Handler&			_db;
		const std::string			_table;
		const std::string			_field;

		// Name, track count, special value that means 'all' if set to 1
		typedef boost::tuple<std::string, int, int>  ResultType;
		Wt::Dbo::QueryModel< ResultType >	_queryModel;

		Wt::WTableView*				_tableView;
};

} // namespace UserInterface

#endif

