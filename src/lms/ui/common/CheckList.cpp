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

#include "CheckList.hpp"

#include <algorithm>

#include <Wt/WAny.h>
#include <Wt/WCheckBox.h>
#include <Wt/WLineEdit.h>

#include "core/String.hpp"

#include "InfiniteScrollingContainer.hpp"

namespace lms::ui
{
    namespace
    {
        // Shorter lists fit on screen
        constexpr int minRowCountForSearch{ 10 };
        // Some lists have thousands of rows (labels for instance)
        constexpr std::size_t batchSize{ 50 };

        Wt::WString getText(const Wt::WAbstractItemModel& model, std::size_t row)
        {
            return Wt::asString(model.data(model.index(static_cast<int>(row), 0)));
        }
    } // namespace

    CheckList::CheckList()
        : Wt::WTemplate{ Wt::WString::tr("Lms.check-list.template") }
    {
        _search = bindNew<Wt::WLineEdit>("search");
        _search->setPlaceholderText(Wt::WString::tr("Lms.Explore.Search.search-placeholder"));
        _search->textInput().connect([this] { applySearch(); });

        _rows = bindNew<InfiniteScrollingContainer>("rows", Wt::WString::tr("Lms.check-list.template.rows"));
        _rows->setNoResultsMessage(Wt::WString::tr("Lms.Explore.no-results"));
        _rows->onRequestElements.connect([this] { addRows(); });
    }

    void CheckList::setModel(std::shared_ptr<Wt::WAbstractItemModel> model)
    {
        _model = std::move(model);
        _checked.assign(static_cast<std::size_t>(_model->rowCount()), false);

        _search->setText("");
        _search->setHidden(_model->rowCount() < minRowCountForSearch);

        applySearch();
    }

    std::vector<std::size_t> CheckList::getCheckedRows() const
    {
        std::vector<std::size_t> rows;
        for (std::size_t row{}; row < _checked.size(); ++row)
        {
            if (_checked[row])
                rows.push_back(row);
        }

        return rows;
    }

    void CheckList::setChecked(std::size_t row, bool checked)
    {
        _checked[row] = checked;

        const auto it{ std::find(std::cbegin(_matchingRows), std::cbegin(_matchingRows) + _checkBoxes.size(), row) };
        if (it != std::cbegin(_matchingRows) + _checkBoxes.size())
            _checkBoxes[it - std::cbegin(_matchingRows)]->setChecked(checked);
    }

    void CheckList::applySearch()
    {
        const std::string query{ _search->text().toUTF8() };

        _matchingRows.clear();
        for (std::size_t row{}; row < _checked.size(); ++row)
        {
            if (core::stringUtils::stringCaseInsensitiveContains(getText(*_model, row).toUTF8(), query))
                _matchingRows.push_back(row);
        }

        _checkBoxes.clear();
        _rows->reset();
        addRows();
    }

    void CheckList::addRows()
    {
        const std::size_t end{ std::min(_checkBoxes.size() + batchSize, _matchingRows.size()) };
        for (std::size_t i{ _checkBoxes.size() }; i < end; ++i)
        {
            const std::size_t row{ _matchingRows[i] };

            auto rowWidget{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.check-list.template.row")) };
            rowWidget->setStyleClass("list-group-item border-0 p-0");
            rowWidget->bindString("text", getText(*_model, row), Wt::TextFormat::Plain);

            Wt::WCheckBox* checkBox{ rowWidget->bindNew<Wt::WCheckBox>("checkbox") };
            checkBox->setChecked(_checked[row]);
            checkBox->changed().connect([this, checkBox, row] {
                _checked[row] = checkBox->isChecked();
                _checkedChanged.emit();
            });
            _checkBoxes.push_back(checkBox);

            _rows->add(std::move(rowWidget));
        }

        _rows->setHasMore(end < _matchingRows.size());
    }
} // namespace lms::ui
