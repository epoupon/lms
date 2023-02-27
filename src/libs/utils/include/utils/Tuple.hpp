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

#include <tuple>

namespace Utils
{
	namespace Details
	{
		template<int... Is>
		struct Seq { };

		template<int N, int... Is>
		struct GenSeq : GenSeq<N - 1, N - 1, Is...> { };

		template<int... Is>
		struct GenSeq<0, Is...> : Seq<Is...> { };

	    template<typename T, typename Func, int... Is>
	    void forEachTypeInTuple(T&& t, Func f, Seq<Is...>)
	    {
	        auto l = { (f(std::get<Is>(t)), 0)... };
	    }
	}

	template<typename... Ts, typename Func>
	void forEachTypeInTuple(std::tuple<Ts...> const& t, Func f)
	{
	    Details::forEachTypeInTuple(t, f, Details::GenSeq<sizeof...(Ts)>());
	}
}


