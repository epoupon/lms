#ifndef SEARCH_FILTER_WIDGET_PP
#define SEARCH_FILTER_WIDGET_PP

#include <Wt/WLineEdit>

#include "FilterWidget.hpp"

class SearchFilterWidget : public FilterWidget {
	public:
		SearchFilterWidget(Wt::WContainerWidget* parent = 0);

		void setText(const std::string& text);

		// Set constraint on this filter
		void refresh(const Constraint& constraint) {}

		// Get constraints created by this filter
		void getConstraint(Constraint& constraint);

	private:

		void handleKeyWentUp(void);

		std::string	 _lastEmittedText;
};





#endif

