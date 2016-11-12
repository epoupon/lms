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

#pragma once

#include <Wt/WContainerWidget>
#include <Wt/WWebWidget>

#include <Wt/WConfig.h>
#if WT_VERSION >= 0X03030500
class InputRange : public Wt::WWebWidget
{
	public:
	InputRange(Wt::WContainerWidget *parent = 0);
	Wt::DomElementType domElementType() const
	{
		return Wt::DomElement_INPUT;
	}

	std::string jsRef() const;
};

#else

class InputRange : public Wt::WContainerWidget
{
	public:
	InputRange(Wt::WContainerWidget *parent = 0);

	std::string jsRef() const;
};

#endif


