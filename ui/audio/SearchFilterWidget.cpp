#include <Wt/WLabel>

#include "SearchFilterWidget.hpp"

namespace UserInterface {

SearchFilterWidget::SearchFilterWidget(Wt::WContainerWidget* parent)
: FilterWidget( parent )
{
}

void
SearchFilterWidget::setText(const std::string& text)
{
	if (text != _lastEmittedText) {
		_lastEmittedText = text;
		emitUpdate();
	}
}

// Get constraints created by this filter
void
SearchFilterWidget::getConstraint(Constraint& constraint)
{
	// No active search means no constaint!
	if (!_lastEmittedText.empty()) {
		const std::string bindText ("%%" + _lastEmittedText + "%%");
		constraint.where.And( WhereClause("(track.name like ? or release.name like ? or artist.name like ?)").bind(bindText).bind(bindText).bind(bindText));
	}

	// else no constraint!
}

} // namespace UserInterface

