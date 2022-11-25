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

#include "services/database/Cluster.hpp"
#include "services/database/Session.hpp"

#include "LmsApplication.hpp"
#include "Utils.hpp"
#include "ModalManager.hpp"

namespace UserInterface {

using namespace Database;

void
Filters::showDialog()
{
	auto dialog {std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.template.add-filter"))};
	Wt::WWidget* dialogPtr {dialog.get()};
	dialog->addFunction("tr", &Wt::WTemplate::Functions::tr);
	dialog->addFunction("id", &Wt::WTemplate::Functions::id);

	Wt::WComboBox* typeCombo {dialog->bindNew<Wt::WComboBox>("type")};
	Wt::WComboBox* valueCombo {dialog->bindNew<Wt::WComboBox>("value")};

	Wt::WPushButton* addBtn {dialog->bindNew<Wt::WPushButton>("add-btn", Wt::WString::tr("Lms.Explore.add-filter"))};
	addBtn->clicked().connect([=]
	{
		const std::string type {typeCombo->valueText().toUTF8()};
		const std::string value {valueCombo->valueText().toUTF8()};

		// TODO use a model to store the cluster.id() values
		ClusterId clusterId {};

		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			ClusterType::pointer clusterType {ClusterType::find(LmsApp->getDbSession(), type)};
			if (!clusterType)
				return;

			Cluster::pointer cluster {clusterType->getCluster(value)};
			if (!cluster)
				return;

			clusterId = cluster->getId();
		}

		add(clusterId);
		LmsApp->getModalManager().dispose(dialogPtr);
	});

	Wt::WPushButton* cancelBtn {dialog->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel"))};
	cancelBtn->clicked().connect([=]
	{
		LmsApp->getModalManager().dispose(dialogPtr);
	});


	// Populate data
	{
		auto transaction {LmsApp->getDbSession().createSharedTransaction()};

		const auto clusterTypesIds {ClusterType::findUsed(LmsApp->getDbSession(), Range {})};
		for (const ClusterTypeId clusterTypeId : clusterTypesIds.results)
		{
			const auto clusterType {ClusterType::find(LmsApp->getDbSession(), clusterTypeId)};
			typeCombo->addItem(Wt::WString::fromUTF8(clusterType->getName()));
		}

		if (!clusterTypesIds.results.empty())
		{
			const auto clusterType {ClusterType::find(LmsApp->getDbSession(), clusterTypesIds.results.front())};

			for (const Cluster::pointer& cluster : clusterType->getClusters())
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

		auto clusterType {ClusterType::find(LmsApp->getDbSession(), name)};
		for (const Cluster::pointer& cluster : clusterType->getClusters())
		{
			if (std::find(std::cbegin(_clusterIds), std::cend(_clusterIds), cluster->getId()) == _clusterIds.end())
				valueCombo->addItem(Wt::WString::fromUTF8(cluster->getName()));
		}
	});

	LmsApp->getModalManager().show(std::move(dialog));
}

void
Filters::add(ClusterId clusterId)
{
	if (std::find(std::cbegin(_clusterIds), std::cend(_clusterIds), clusterId) != std::cend(_clusterIds))
		return;

	Wt::WInteractWidget* filter {};

	{
		auto cluster {Utils::createCluster(clusterId, true)};
		if (!cluster)
			return;

		filter = _filters->addWidget(std::move(cluster));
	}

	_clusterIds.push_back(clusterId);

	filter->clicked().connect([=]
	{
		_filters->removeWidget(filter);
		_clusterIds.erase(std::remove_if(std::begin(_clusterIds), std::end(_clusterIds), [clusterId](ClusterId id) { return id == clusterId; }), std::end(_clusterIds));
		_sigUpdated.emit();
	});

	LmsApp->notifyMsg(Notification::Type::Info,
			Wt::WString::tr("Lms.Explore.filters"),
			Wt::WString::tr("Lms.Explore.filter-added"), std::chrono::seconds {2});

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

