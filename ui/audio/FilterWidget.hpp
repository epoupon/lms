#ifndef FILTER_WIDGET_HPP
#define FILTER_WIDGET_HPP

#include <boost/foreach.hpp>
#include <string>
#include <list>

#include <Wt/WSignal>
#include <Wt/WContainerWidget>

#include "database/SqlQuery.hpp"

class FilterWidget : public Wt::WContainerWidget {

	public:

		struct Constraint {
			WhereClause	where;		// WHERE SQL clause
		};

		FilterWidget(Wt::WContainerWidget* parent = 0) : Wt::WContainerWidget(parent) {}
		virtual ~FilterWidget() {}

		// Refresh filter Widget using constraints created by parent filters
		virtual void refresh(const Constraint& constraint) = 0;

		// Update constraints for child filters
		virtual void getConstraint(Constraint& constraint) = 0;

		// Emitted when a constraint has changed
		Wt::Signal<void>& update() { return _update; };

	protected:

		void emitUpdate()	{ _update.emit(); }

	private:

		Wt::Signal<void> _update;
};





#endif
