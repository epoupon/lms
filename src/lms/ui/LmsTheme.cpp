/*
 * Copyright (C) 2020 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "LmsTheme.hpp"

#include <Wt/WApplication.h>
#include <Wt/WLinkedCssStyleSheet.h>

namespace UserInterface
{

	void
	LmsTheme::init(Wt::WApplication* app) const
	{
		app->require("js/bootstrap.bundle.min.js");
	}

	std::string
	LmsTheme::name() const
	{
		return "bootstrap5";
	}

	std::string
	LmsTheme::resourcesUrl() const
	{
		return "";
	}

	std::vector<Wt::WLinkedCssStyleSheet>
	LmsTheme::styleSheets() const
	{
		static const std::vector<Wt::WLinkedCssStyleSheet> files
		{
			{"css/bootstrap.solar.min.css"}, // TODO parametrize this
			{"css/lms.css"},
		};

		return files;
	}

	void LmsTheme::applyValidationStyle(Wt::WWidget* widget, const Wt::WValidator::Result& validation, Wt::WFlags<Wt::ValidationStyleFlag> styles) const
	{
		{
			const bool validStyle {(validation.state() == Wt::ValidationState::Valid) && styles.test(Wt::ValidationStyleFlag::ValidStyle)};
			widget->toggleStyleClass("is-valid", validStyle);
		}

		{
			const bool invalidStyle  {(validation.state() != Wt::ValidationState::Valid) &&  styles.test(Wt::ValidationStyleFlag::InvalidStyle)};
			widget->toggleStyleClass("is-invalid", invalidStyle);
		}
	}

} // namespace UserInterface

