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

#include "InfiniteScrollingContainer.hpp"

#include <cassert>
#include "LoadingIndicator.hpp"

namespace UserInterface
{
	InfiniteScrollingContainer::InfiniteScrollingContainer(const Wt::WString& text)
		: Wt::WTemplate {text}
		, _elements {bindNew<Wt::WContainerWidget>("elements")}
		, _loadingIndicator {bindWidget<Wt::WTemplate>("loading-indicator", createLoadingIndicator())}
	{
		reset();
	}

	void
	InfiniteScrollingContainer::clear()
	{
		assert(false);
	}

	void
	InfiniteScrollingContainer::reset()
	{
		_elements->clear();
		setHasMore(true);
	}

	std::size_t
	InfiniteScrollingContainer::getCount()
	{
		return _elements->count();
	}

	void
	InfiniteScrollingContainer::add(std::unique_ptr<Wt::WWidget> result)
	{
		return _elements->addWidget(std::move(result));
	}

	void
	InfiniteScrollingContainer::setHasMore()
	{
		setHasMore(true);
	}

	void
	InfiniteScrollingContainer::setHasMore(bool hasMore)
	{
		if (hasMore)
			displayLoadingIndicator();
		else
			hideLoadingIndicator();
	}

	void
	InfiniteScrollingContainer::remove(Wt::WWidget& widget)
	{
		_elements->removeWidget(&widget);
	}

	void
	InfiniteScrollingContainer::remove(std::size_t first, std::size_t last)
	{
		assert(last >= first);
		assert(last < static_cast<std::size_t>(_elements->count()));

		// remove from end as API is quite uneffective (minimize moves)
		std::size_t i {last};
		while (Wt::WWidget* widget {_elements->widget(i)})
		{
			_elements->removeWidget(widget);
			if (i-- == first)
				break;
		}
	}

	Wt::WWidget*
	InfiniteScrollingContainer::getWidget(std::size_t pos) const
	{
		return _elements->widget(pos);
	}

	std::optional<std::size_t>
	InfiniteScrollingContainer::getIndexOf(Wt::WWidget& widget) const
	{
		return _elements->indexOf(&widget);
	}


	void
	InfiniteScrollingContainer::displayLoadingIndicator()
	{
		_loadingIndicator = bindWidget<Wt::WTemplate>("loading-indicator", createLoadingIndicator());
		_loadingIndicator->scrollVisibilityChanged().connect([this](bool visible)
		{
			if (!visible)
				return;

			const auto previousCount {_elements->count()};
			onRequestElements.emit();
			setHasMore(previousCount != _elements->count());
		});
	}

	void
	InfiniteScrollingContainer::hideLoadingIndicator()
	{
		_loadingIndicator = nullptr;
		bindEmpty("loading-indicator");
	}

}
