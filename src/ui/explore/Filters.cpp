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

#include "Filters.hpp"

#include <Wt/WComboBox.h>
#include <Wt/WDialog.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplate.h>

#include "LmsApplication.hpp"

namespace UserInterface {

void
Filters::showDialog()
{
	auto dialog = std::make_shared<Wt::WDialog>(Wt::WString::tr("Lms.Explore.add-filter"));

	Wt::WTemplate* container = dialog->contents()->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.template.add-filter"));
	container->addFunction("tr", &Wt::WTemplate::Functions::tr);

	Wt::WComboBox* typeCombo = container->bindNew<Wt::WComboBox>("type");
	Wt::WComboBox* valueCombo = container->bindNew<Wt::WComboBox>("value");

	Wt::WPushButton* addBtn = container->bindNew<Wt::WPushButton>("add-btn", Wt::WString::tr("Lms.add"));
	addBtn->clicked().connect(dialog.get(), &Wt::WDialog::accept);

	Wt::WPushButton* cancelBtn = container->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel"));
	cancelBtn->clicked().connect(dialog.get(), &Wt::WDialog::reject);

	// Populate data
	{
		Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

		auto types = Database::ClusterType::getAll(LmsApp->getDboSession());

		for (auto type : types)
			typeCombo->addItem(Wt::WString::fromUTF8(type->getName()));

		if (!types.empty())
		{
			auto values = types.front()->getClusters();

			for (auto value : values)
			{
				if (_filterIds.find(value.id()) == _filterIds.end())
					valueCombo->addItem(Wt::WString::fromUTF8(value->getName()));
			}
		}

	}

	typeCombo->changed().connect(std::bind([=]
	{
		auto name = typeCombo->valueText().toUTF8();

		valueCombo->clear();

		Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

		auto clusterType = Database::ClusterType::getByName(LmsApp->getDboSession(), name);

		auto values = clusterType->getClusters();
		for (auto value : values)
		{
			if (_filterIds.find(value.id()) == _filterIds.end())
				valueCombo->addItem(Wt::WString::fromUTF8(value->getName()));
		}
	}));

	dialog->setModal(true);
	dialog->setMovable(false);

	dialog->setResizable(false);
	dialog->setClosable(false);

	dialog->finished().connect(std::bind([=]
	{
		if (dialog->result() != Wt::DialogCode::Accepted)
			return;

		auto type = typeCombo->valueText().toUTF8();
		auto value = valueCombo->valueText().toUTF8();

		Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

		auto clusterType = Database::ClusterType::getByName(LmsApp->getDboSession(), type);
		if (!clusterType)
			return;

		auto cluster = clusterType->getCluster(value);
		if (!cluster)
			return;

		add(cluster.id());
	}));

	dialog->show();
}

void
Filters::add(Database::IdType clusterId)
{
	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	auto cluster = Database::Cluster::getById(LmsApp->getDboSession(), clusterId);
	if (!cluster)
		return;

	auto res = _filterIds.insert(clusterId);
	if (!res.second)
		return;

	auto filter = _filters->addWidget(LmsApp->createCluster(cluster, true));
	filter->clicked().connect(std::bind([=]
	{
		_filters->removeWidget(filter);
		_filterIds.erase(clusterId);
		_sigUpdated.emit();
	}));

	_sigUpdated.emit();
}

Filters::Filters()
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.template.filters"))
{
	// Filters
	Wt::WPushButton *addFilterBtn = bindNew<Wt::WPushButton>("add-filter", Wt::WText::tr("Lms.Explore.add-filter"));
	addFilterBtn->clicked().connect(this, &Filters::showDialog);

	_filters = bindNew<Wt::WContainerWidget>("clusters");
}



} // namespace UserInterface

