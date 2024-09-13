/*
 * Copyright (C) 2024 Emeric Poupon
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

#include <string>

#include <Wt/WPushButton.h>
#include <Wt/WSignal.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>

namespace lms::ui
{
    template<typename ItemType>
    class DropDownMenuSelector : public Wt::WTemplate
    {
    public:
        // text is the template of the dropdown menu
        // "selected-item" is the key for the currently selected entry
        DropDownMenuSelector(const Wt::WString& text, ItemType defaultItem)
            : Wt::WTemplate{ text }
            , _defaultItem{ defaultItem }
        {
            _selectedItem = bindNew<Wt::WText>("selected-item");
        }

        void bindItem(const std::string& var, const Wt::WString& title, ItemType item)
        {
            auto* menuItem{ bindNew<Wt::WPushButton>(var, title) };
            menuItem->clicked().connect([this, menuItem, title, item] {
                if (_currentActiveItem)
                    _currentActiveItem->removeStyleClass("active");

                menuItem->addStyleClass("active");
                _currentActiveItem = menuItem;
                _selectedItem->setText(title);

                itemSelected.emit(item);
            });

            if (item == _defaultItem)
            {
                _currentActiveItem = menuItem;
                _currentActiveItem->addStyleClass("active");
                _selectedItem->setText(title);
            }
        }

        Wt::Signal<ItemType> itemSelected;

    private:
        const ItemType _defaultItem;
        Wt::WWidget* _currentActiveItem{};
        Wt::WText* _selectedItem{};
    };
} // namespace lms::ui