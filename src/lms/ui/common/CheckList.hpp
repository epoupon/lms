/*
 * Copyright (C) 2026 Emeric Poupon
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
#include <vector>

#include <Wt/WAbstractItemModel.h>
#include <Wt/WSignal.h>
#include <Wt/WTemplate.h>

namespace Wt
{
    class WCheckBox;
    class WLineEdit;
} // namespace Wt

namespace lms::ui
{
    class InfiniteScrollingContainer;

    // List of checkable rows, with a search field on long lists
    class CheckList final : public Wt::WTemplate
    {
    public:
        CheckList();

        // One row per model row, using its display text
        void setModel(std::shared_ptr<Wt::WAbstractItemModel> model);
        const std::shared_ptr<Wt::WAbstractItemModel>& getModel() const { return _model; }

        std::vector<std::size_t> getCheckedRows() const;
        void setChecked(std::size_t row, bool checked);

        // Emitted when the user checks or unchecks a row
        Wt::Signal<>& checkedChanged() { return _checkedChanged; }

    private:
        void applySearch();
        void addRows();

        Wt::WLineEdit* _search{};
        InfiniteScrollingContainer* _rows{};
        std::shared_ptr<Wt::WAbstractItemModel> _model;
        std::vector<bool> _checked;              // indexed by model row
        std::vector<std::size_t> _matchingRows;  // model rows matching the search
        std::vector<Wt::WCheckBox*> _checkBoxes; // displayed rows, the first ones of _matchingRows
        Wt::Signal<> _checkedChanged;
    };
} // namespace lms::ui
