/*
 * Copyright (C) 2015 Emeric Poupon
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

#include <Wt/WConfig.h>

#if WT_VERSION < 0X03030500
#include <Wt/WTemplate>
#endif

#include "InputRange.hpp"

InputRange::InputRange(Wt::WContainerWidget *parent)
: Wt::WWebWidget(parent)
{
	//TODO woraround for older Wt versions
#if WT_VERSION >= 0X03030500
	setHtmlTagName("input");
	setAttributeValue("type", "range");
#else
	Wt::WTemplate *t = new Wt::WTemplate("<input type=\"range\"></input>", parent);
#endif
}


std::string
InputRange::jsRef(void) const
{
#if WT_VERSION >= 0X03030500
	return Wt::WWebWidget::jsRef();
#else
	return this->jsRef() + ".getElementsByTagName(\"input\")[0]";
#endif
}
