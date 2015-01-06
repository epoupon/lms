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

#include <Wt/WTimer>

#include "LineEdit.hpp"

namespace UserInterface {


LineEdit::LineEdit(std::size_t ms, Wt::WContainerWidget* parent)
: Wt::WLineEdit(parent)
{

	Wt::WTimer *timer = new Wt::WTimer(this);
	timer->setSingleShot(true);
	timer->setInterval(ms);

	this->keyWentUp().connect(std::bind([=] (Wt::WKeyEvent keyEvent)
	{
		if (timer->isActive())
			timer->stop();

		if (keyEvent.key() == Wt::Key_Enter)
			_sigTimedChanged.emit(this->text());
		else
			timer->start();
	}, std::placeholders::_1));

	timer->timeout().connect(std::bind([=] ()
	{
		_sigTimedChanged.emit(this->text());
	}));
}


} // namespace UserInterface


