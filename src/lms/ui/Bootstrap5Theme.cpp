/*
 * Copyright (C) 2020 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Bootstrap5Theme.h"

#include <Wt/WApplication.h>
#include <Wt/WLinkedCssStyleSheet.h>
#include <Wt/WText.h>

namespace UserInterface
{

	void
	Bootstrap5Theme::init(Wt::WApplication* app) const
	{
		app->require(resourcesUrl() + "js/bootstrap.bundle.min.js");
		Wt::WString v = app->metaHeader(Wt::MetaHeaderType::Meta, "viewport");
		if (v.empty())
			app->addMetaHeader("viewport", "width=device-width, initial-scale=1");
	}

	std::string
	Bootstrap5Theme::name() const
	{
		return "bootstrap5";
	}

	std::string
	Bootstrap5Theme::resourcesUrl() const
	{
		return Wt::WApplication::relativeResourcesUrl() + "themes/bootstrap/5/";
	}

	std::vector<Wt::WLinkedCssStyleSheet>
	Bootstrap5Theme::styleSheets() const
	{
		std::vector<Wt::WLinkedCssStyleSheet> result;

		const std::string themeDir {resourcesUrl()};

		result.emplace_back(Wt::WLink {themeDir + "css/bootstrap.min.css"});
		result.emplace_back(Wt::WLink {themeDir + "wt.css"});

		return result;
	}

	std::string
	Bootstrap5Theme::utilityCssClass(int utilityCssClassRole) const
	{
		switch (utilityCssClassRole) {
			case Wt::ToolTipInner:
				return "tooltip-inner";
			case Wt::ToolTipOuter:
				return "tooltip fade top in";
			default:
				return "";
		}
	}

	void Bootstrap5Theme::applyValidationStyle(Wt::WWidget* widget, const Wt::WValidator::Result& validation, Wt::WFlags<Wt::ValidationStyleFlag> styles) const
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

