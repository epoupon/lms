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

#include <sstream>

#include <Wt/WAny.h>
#include <Wt/WCheckBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLineEdit.h>

namespace lms::ui
{
    namespace
    {
        // Shorter lists fit on screen
        constexpr int minRowCountForSearch{ 10 };
    } // namespace

    CheckList::CheckList()
        : Wt::WTemplate{ Wt::WString::tr("Lms.check-list.template") }
    {
        _search = bindNew<Wt::WLineEdit>("search");
        _search->setPlaceholderText(Wt::WString::tr("Lms.Explore.Search.search-placeholder"));
        _rows = bindNew<Wt::WContainerWidget>("rows");

        // Filtered in the browser, no round trip on each key stroke
        std::ostringstream oss;
        oss << "function(o) {"
            << " const query = o.value.toLowerCase();"
            << " for (const row of document.getElementById('" << _rows->id() << "').children)"
            << "  row.style.display = row.textContent.toLowerCase().includes(query) ? '' : 'none';"
            << "}";
        _search->textInput().connect(oss.str());
    }

    void CheckList::setModel(std::shared_ptr<Wt::WAbstractItemModel> model)
    {
        _model = std::move(model);

        _rows->clear();
        _checkBoxes.clear();
        _search->setText("");

        for (int row{}; row < _model->rowCount(); ++row)
        {
            // The padding is on the inner label, so that a click anywhere on the row toggles its checkbox
            // (setHtmlTagName("label") would break: Wt cannot create a label tag on later updates)
            auto rowWidget{ std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.check-list.template.row")) };
            rowWidget->setStyleClass("list-group-item p-0");
            rowWidget->bindString("text", Wt::asString(_model->data(_model->index(row, 0))), Wt::TextFormat::Plain);

            Wt::WCheckBox* checkBox{ rowWidget->bindNew<Wt::WCheckBox>("checkbox") };
            checkBox->changed().connect([this] { _checkedChanged.emit(); });
            _checkBoxes.push_back(checkBox);

            _rows->addWidget(std::move(rowWidget));
        }

        _search->setHidden(_model->rowCount() < minRowCountForSearch);
    }

    std::vector<std::size_t> CheckList::getCheckedRows() const
    {
        std::vector<std::size_t> rows;
        for (std::size_t row{}; row < _checkBoxes.size(); ++row)
        {
            if (_checkBoxes[row]->isChecked())
                rows.push_back(row);
        }

        return rows;
    }

    void CheckList::setChecked(std::size_t row, bool checked)
    {
        _checkBoxes[row]->setChecked(checked);
    }
} // namespace lms::ui
