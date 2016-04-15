/*
 * Copyright (C) 2016 Emeric Poupon
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

#include <Wt/Dbo/Dbo>

namespace Database {

class Track;

class ClassificationData
{
	public:
		typedef Wt::Dbo::ptr<ClassificationData> pointer;

		ClassificationData() {}

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _type,	"type");
				Wt::Dbo::field(a, _value,	"value");
				Wt::Dbo::belongsTo(a,	_track, "track", Wt::Dbo::OnDeleteCascade);
			}

	private:
		std::string	_type;
		std::string	_value;

		Wt::Dbo::ptr<Track>	_track;
};

class Classification
{
	public:
		typedef Wt::Dbo::ptr<Classification> pointer;

		Classification() {}

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _type, 	"type");
				Wt::Dbo::field(a, _value,	"value");
				Wt::Dbo::belongsTo(a,	_track, "track", Wt::Dbo::OnDeleteCascade);
			}

	private:

		std::string	_type;
		int		_value;

		Wt::Dbo::ptr<Track>	_track;
};

} // namespace Database

