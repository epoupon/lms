/*
 * Copyright (C) 2022 Emeric Poupon
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

#include <Wt/WTheme.h>

namespace UserInterface
{
	class LmsTheme : public Wt::WTheme
	{
		private:
			void init(Wt::WApplication *app) const override;

			std::string name() const override;
			std::string resourcesUrl() const override;
			std::vector<Wt::WLinkedCssStyleSheet> styleSheets() const override;
			void apply(Wt::WWidget*, Wt::WWidget*, int) const override {};
			void apply(Wt::WWidget*, Wt::DomElement&, int) const override {};
			std::string disabledClass() const override { return "disabled"; }
			std::string activeClass() const override { return "active"; };
			std::string utilityCssClass(int) const override { return ""; };
			bool canStyleAnchorAsButton() const override { return true; };
			void applyValidationStyle(Wt::WWidget* widget,
					const Wt::WValidator::Result& validation,
					Wt::WFlags<Wt::ValidationStyleFlag> flags) const override;
			bool canBorderBoxElement(const Wt::DomElement&) const override { return true; }
	};
}
