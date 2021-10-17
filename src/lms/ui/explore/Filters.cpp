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
#include "database/Session.hpp"

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

	Wt::WPushButton* addBtn = container->bindNew<Wt::WPushButton>("add-btn", Wt::WString::tr("Lms.Explore.add-filter"));
	addBtn->clicked().connect(dialog.get(), &Wt::WDialog::accept);

	Wt::WPushButton* cancelBtn = container->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel"));
	cancelBtn->clicked().connect(dialog.get(), &Wt::WDialog::reject);

	// Populate data
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const auto types {Database::ClusterType::getAllUsed(LmsApp->getDbSession())};
		for (const Database::ClusterType::pointer& type : types)
			typeCombo->addItem(Wt::WString::fromUTF8(type->getName()));

		if (!types.empty())
		{
			for (const Database::Cluster::pointer& cluster : types.front()->getClusters())
			{
				if (std::find(std::cbegin(_clusterIds), std::cend(_clusterIds), cluster->getId()) == _clusterIds.end())
					valueCombo->addItem(Wt::WString::fromUTF8(cluster->getName()));
			}
		}
	}

	typeCombo->changed().connect([=]
	{
		const std::string name {typeCombo->valueText().toUTF8()};

		valueCombo->clear();

		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		auto clusterType = Database::ClusterType::getByName(LmsApp->getDbSession(), name);

		for (const Database::Cluster::pointer& cluster : clusterType->getClusters())
		{
			if (std::find(std::cbegin(_clusterIds), std::cend(_clusterIds), cluster->getId()) == _clusterIds.end())
				valueCombo->addItem(Wt::WString::fromUTF8(cluster->getName()));
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

		// TODO use a model to store the cluster.id() values
		Database::ClusterId clusterId {};

		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			Database::ClusterType::pointer clusterType {Database::ClusterType::getByName(LmsApp->getDbSession(), type)};
			if (!clusterType)
				return;

			Database::Cluster::pointer cluster {clusterType->getCluster(value)};
			if (!cluster)
				return;

			clusterId = cluster->getId();
		}

		add(clusterId);
	});

	dialog->show();
}

void
Filters::add(Database::ClusterId clusterId)
{

	Wt::WInteractWidget* filter {};

	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		Database::Cluster::pointer cluster {Database::Cluster::getById(LmsApp->getDbSession(), clusterId)};
		if (!cluster)
			return;

		if (std::find(std::cbegin(_clusterIds), std::cend(_clusterIds), clusterId) != std::cend(_clusterIds))
			return;

		_clusterIds.push_back(clusterId);

		filter = _filters->addWidget(LmsApp->createCluster(cluster, true));
	}

	filter->clicked().connect([=]
	{
		_filters->removeWidget(filter);
		_clusterIds.erase(std::remove_if(std::begin(_clusterIds), std::end(_clusterIds), [clusterId](Database::ClusterId id) { return id == clusterId; }), std::end(_clusterIds));
		_sigUpdated.emit();
	});

	LmsApp->notifyMsg(LmsApplication::MsgType::Info, Wt::WString::tr("Lms.Explore.filter-added"), std::chrono::seconds {2});

	_sigUpdated.emit();
}

Filters::Filters()
: Wt::WTemplate {Wt::WString::tr("Lms.Explore.template.filters")}
{
	addFunction("tr", &Functions::tr);

	// Filters
	Wt::WPushButton *addFilterBtn = bindNew<Wt::WPushButton>("add-filter", Wt::WText::tr("Lms.Explore.add-filter"));
	addFilterBtn->clicked().connect(this, &Filters::showDialog);

	_filters = bindNew<Wt::WContainerWidget>("clusters");
}



} // namespace UserInterface

