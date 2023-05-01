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

#include <memory>
#include <optional>
#include <utility>

#include <Wt/WContainerWidget.h>
#include <Wt/WSignal.h>
#include <Wt/WString.h>
#include <Wt/WTemplate.h>

namespace UserInterface
{
	// Atomatically raises onRequestElements signal when the sentinel is displayed
	// can add elements afterwards by calling setHasMoreElements()
	class InfiniteScrollingContainer final : public Wt::WTemplate
	{
		public:
			// "text" must contain loading-indicator and "elements"
			InfiniteScrollingContainer(const Wt::WString& text = Wt::WString::tr("Lms.infinite-scrolling-container.template"));

			void reset();
			std::size_t getCount();
			void add(std::unique_ptr<Wt::WWidget> result);

			template<typename T, typename... Args>
			T* addNew(Args&&... args)
			{
				return _elements->addNew<T>(std::forward<Args>(args)...);
			}

			void remove(Wt::WWidget& widget);
			void remove(std::size_t first, std::size_t last);

			Wt::WWidget*				getWidget(std::size_t pos) const;
			std::optional<std::size_t>	getIndexOf(Wt::WWidget& widget) const;
			void 						setHasMore(); // can be used to add elements afterwards

			Wt::Signal<>	onRequestElements;

		private:
			void clear() override;
			void displayLoadingIndicator();
			void hideLoadingIndicator();
			void setHasMore(bool hasMore); // can be used to add elements afterwards

			Wt::WContainerWidget*	_elements;
			Wt::WTemplate*			_loadingIndicator;
	};
}
