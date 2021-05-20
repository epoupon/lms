/*
 * Copyright (C) 2021 Emeric Poupon
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

#include <optional>
#include <utility>

#include <Wt/WContainerWidget.h>
#include <Wt/WSignal.h>
#include <Wt/WString.h>
#include <Wt/WTemplate.h>

namespace UserInterface
{
	class InfiniteScrollingContainer : public Wt::WTemplate
	{
		public:
			// "text" must contain loading-indicator and "elements"
			InfiniteScrollingContainer(const Wt::WString& text = Wt::WString::tr("Lms.infinite-scrolling-container"));

			void clear();
			std::size_t getCount();
			void add(std::unique_ptr<Wt::WWidget> result);

			template<typename T, typename... Args>
			T* addNew(Args&&... args)
			{
				return _elements->addNew<T>(std::forward<Args>(args)...);
			}

			void remove(Wt::WWidget& widget);

			Wt::WWidget*				getWidget(std::size_t pos) const;
			std::optional<std::size_t>	getIndexOf(Wt::WWidget& widget) const;

			void setHasMore(bool hasMore);

			Wt::Signal<>	onRequestElements;

		private:
			void displayLoadingIndicator();
			void hideLoadingIndicator();

			Wt::WContainerWidget*	_elements;
			Wt::WTemplate*			_loadingIndicator;
	};
}
