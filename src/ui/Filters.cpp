/*
 * Copyright (C) 2018 Emeric Poupon
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

#include <Wt/WComboBox>
#include <Wt/WDialog>
#include <Wt/WPushButton>
#include <Wt/WTemplate>

#include "Filters.hpp"

#include "LmsApplication.hpp"

namespace UserInterface {

void
Filters::showDialog()
{
	auto dialog = new Wt::WDialog("Go to cell");

	auto container = new Wt::WTemplate(Wt::WString::tr("template-filter-add"));
	container->addFunction("tr", &Wt::WTemplate::Functions::tr);

	dialog->contents()->addWidget(container);

	auto typeCombo = new Wt::WComboBox();
	container->bindWidget("type", typeCombo);

	auto valueCombo = new Wt::WComboBox();
	container->bindWidget("value", valueCombo);

	auto addBtn = new Wt::WPushButton(Wt::WString::tr("msg-add"));
	container->bindWidget("add", addBtn);
	addBtn->clicked().connect(dialog, &Wt::WDialog::accept);

	auto cancelBtn = new Wt::WPushButton(Wt::WString::tr("msg-cancel"));
	container->bindWidget("cancel", cancelBtn);
	cancelBtn->clicked().connect(dialog, &Wt::WDialog::reject);

	// Populate data
	{
		Wt::Dbo::Transaction transaction(DboSession());

		auto types = Database::Cluster::getAllTypes(DboSession());

		for (auto type : types)
			typeCombo->addItem(Wt::WString::fromUTF8(type));

		if (!types.empty())
		{
			auto values = Database::Cluster::getByType(DboSession(), types.front());

			for (auto value : values)
			{
				if (_filterIds.find(value.id()) == _filterIds.end())
					valueCombo->addItem(Wt::WString::fromUTF8(value->getName()));
			}
		}

	}

	typeCombo->changed().connect(std::bind([=]
	{
		auto type = typeCombo->valueText().toUTF8();

		valueCombo->clear();

		Wt::Dbo::Transaction transaction(DboSession());

		auto values = Database::Cluster::getByType(DboSession(), type);
		for (auto value : values)
		{
			if (_filterIds.find(value.id()) == _filterIds.end())
				valueCombo->addItem(Wt::WString::fromUTF8(value->getName()));
		}
	}));

	dialog->setModal(true);
#if WT_VERSION >= 0x03030700
	dialog->setMovable(false);
#endif

	dialog->setResizable(false);
	dialog->setClosable(false);

	dialog->finished().connect(std::bind([=]
	{
		if (dialog->result() != Wt::WDialog::Accepted)
			return;

		auto type = typeCombo->valueText().toUTF8();
		auto value = valueCombo->valueText().toUTF8();

		Wt::Dbo::Transaction transaction(DboSession());

		auto cluster = Database::Cluster::get(DboSession(), type, value);
		if (!cluster)
			return;

		auto clusterId = cluster.id();
		_filterIds.insert(clusterId);
		_sigUpdated.emit();

		auto filterBtn = new Wt::WPushButton(Wt::WString::fromUTF8(value));
		_filters->addWidget(filterBtn);

		filterBtn->clicked().connect(std::bind([=]
		{
			_filters->removeWidget(filterBtn);
			_filterIds.erase(clusterId);
			_sigUpdated.emit();
		}));
	}));

	dialog->show();
}

Filters::Filters(Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent)
{
	auto container = new Wt::WTemplate(Wt::WString::tr("template-filters"), this);

	// Filters
	Wt::WPushButton *addFilterBtn = new Wt::WPushButton(Wt::WText::tr("msg-filter-add"));
	container->bindWidget("add-filter", addFilterBtn);

	_filters = new Wt::WContainerWidget();
	container->bindWidget("filters", _filters);

	addFilterBtn->clicked().connect(std::bind([this]
	{
		showDialog();
	}));

}



} // namespace UserInterface

