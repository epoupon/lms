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

#include "database/Cluster.hpp"

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
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const auto types {Database::ClusterType::getAll(LmsApp->getDbSession())};
		for (const Database::ClusterType::pointer& type : types)
			typeCombo->addItem(Wt::WString::fromUTF8(type->getName()));

		if (!types.empty())
		{
			const auto values {types.front()->getClusters()};

			for (const Database::Cluster::pointer& value : values)
			{
				if (_filterIds.find(value.id()) == _filterIds.end())
					valueCombo->addItem(Wt::WString::fromUTF8(value->getName()));
			}
		}
	}

	typeCombo->changed().connect([=]
	{
		const std::string name {typeCombo->valueText().toUTF8()};

		valueCombo->clear();

		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		auto clusterType = Database::ClusterType::getByName(LmsApp->getDbSession(), name);

		const auto values = clusterType->getClusters();
		for (const Database::Cluster::pointer& value : values)
		{
			if (_filterIds.find(value.id()) == _filterIds.end())
				valueCombo->addItem(Wt::WString::fromUTF8(value->getName()));
		}
	});

	dialog->setModal(true);
	dialog->setMovable(false);

	dialog->setResizable(false);
	dialog->setClosable(false);

	dialog->finished().connect([=]
	{
		if (dialog->result() != Wt::DialogCode::Accepted)
			return;

		const std::string type {typeCombo->valueText().toUTF8()};
		const std::string value {valueCombo->valueText().toUTF8()};

		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		Database::ClusterType::pointer clusterType {Database::ClusterType::getByName(LmsApp->getDbSession(), type)};
		if (!clusterType)
			return;

		Database::Cluster::pointer cluster {clusterType->getCluster(value)};
		if (!cluster)
			return;

		add(cluster.id());
	});

	dialog->show();
}

void
Filters::add(Database::IdType clusterId)
{
	auto transaction {LmsApp->getDbSession().createSharedTransaction()};

	Database::Cluster::pointer cluster {Database::Cluster::getById(LmsApp->getDbSession(), clusterId)};
	if (!cluster)
		return;

	auto res {_filterIds.insert(clusterId)};
	if (!res.second)
		return;

	auto filter {_filters->addWidget(LmsApp->createCluster(cluster, true))};
	filter->clicked().connect(std::bind([=]
	{
		_filters->removeWidget(filter);
		_filterIds.erase(clusterId);
		_sigUpdated.emit();
	}));

	LmsApp->notifyMsg(MsgType::Info, Wt::WString::tr("Lms.Explore.filter-added"), std::chrono::seconds {2});

	_sigUpdated.emit();
}

Filters::Filters()
: Wt::WTemplate(Wt::WString::tr("Lms.Explore.template.filters"))
{
	addFunction("tr", &Functions::tr);

	// Filters
	Wt::WPushButton *addFilterBtn = bindNew<Wt::WPushButton>("add-filter", Wt::WText::tr("Lms.add"));
	addFilterBtn->clicked().connect(this, &Filters::showDialog);

	_filters = bindNew<Wt::WContainerWidget>("clusters");
}



} // namespace UserInterface

