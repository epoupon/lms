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

#include "SubsonicView.hpp"

#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WPushButton.h>
#include <Wt/WTemplateFormView.h>

#include <Wt/WFormModel.h>

#include "database/SubsonicSettings.hpp"
#include "utils/Logger.hpp"

#include "common/ValueStringModel.hpp"
#include "LmsApplication.hpp"

namespace UserInterface {

using namespace Database;

class SubsonicModel : public Wt::WFormModel
{

	public:
		static inline const Field EnableField {"enable"};
		static inline const Field ArtistListModeField {"artist-list-mode"};

		SubsonicModel()
		{
			addField(EnableField);
			addField(ArtistListModeField);

			initializeModels();

			loadData();
		}

		std::shared_ptr<Wt::WAbstractItemModel> getArtistListModeModel() { return _artistListModeModel; }

		void saveData()
		{
			auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

			auto settings {SubsonicSettings::get(LmsApp->getDbSession())};

			settings.modify()->setAPIEnabled(Wt::asNumber(value(EnableField)));
			auto artistListModeRow {_artistListModeModel->getRowFromString(valueText(ArtistListModeField))};
			if (artistListModeRow)
				settings.modify()->setArtistListMode(_artistListModeModel->getValue(*artistListModeRow));
		}

		void loadData()
		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};

			const auto settings {SubsonicSettings::get(LmsApp->getDbSession())};

			setValue(EnableField, settings->isAPIEnabled());
			auto artistListModeRow {_artistListModeModel->getRowFromValue(settings->getArtistListMode())};

			if (artistListModeRow)
				setValue(ArtistListModeField, _artistListModeModel->getString(*artistListModeRow));
		}

		void initializeModels()
		{
			_artistListModeModel = std::make_shared<ValueStringModel<SubsonicSettings::ArtistListMode>>();

			_artistListModeModel->add(Wt::WString::tr("Lms.Admin.Subsonic.all-artists"), SubsonicSettings::ArtistListMode::AllArtists);
			_artistListModeModel->add(Wt::WString::tr("Lms.Admin.Subsonic.release-artists"), SubsonicSettings::ArtistListMode::ReleaseArtists);
		}

	private:

		bool validateField(Field field) override
		{
			if (field == EnableField)
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				if (SubsonicSettings::get(LmsApp->getDbSession())->isAPIEnabled() != Wt::asNumber(value(EnableField)))
					setValidation(field, Wt::WValidator::Result( Wt::ValidationState::Valid, Wt::WString::tr("Lms.Admin.Subsonic.need-restart")));
				else
					setValidation(field, Wt::WValidator::Result( Wt::ValidationState::Valid));

				return true;
			}

			return Wt::WFormModel::validateField(field);
		}

		std::shared_ptr<ValueStringModel<SubsonicSettings::ArtistListMode>> _artistListModeModel;
};

SubsonicView::SubsonicView()
{
	wApp->internalPathChanged().connect([this]()
	{
		refreshView();
	});

	refreshView();
}

void
SubsonicView::refreshView()
{
	if (!wApp->internalPathMatches("/admin/subsonic"))
		return;

	clear();

	Wt::WTemplateFormView* t {addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Admin.Subsonic.template"))};

	auto model {std::make_shared<SubsonicModel>()};

	// Enable
	auto enable {std::make_unique<Wt::WCheckBox>()};
	auto* enableRaw {enable.get()};
	t->setFormWidget(SubsonicModel::EnableField, std::move(enable));

	// Artis tlist mode
	auto artistListMode = std::make_unique<Wt::WComboBox>();
	artistListMode->setModel(model->getArtistListModeModel());
	t->setFormWidget(SubsonicModel::ArtistListModeField, std::move(artistListMode));

	enableRaw->changed().connect([=]()
	{
		bool enable {enableRaw->checkState() == Wt::CheckState::Checked};
		model->setReadOnly(SubsonicModel::ArtistListModeField, !enable);
		t->updateModel(model.get());
		t->updateView(model.get());
	});

	Wt::WPushButton* saveBtn = t->bindNew<Wt::WPushButton>("apply-btn", Wt::WString::tr("Lms.apply"));
	Wt::WPushButton *discardBtn {t->bindWidget("discard-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.discard")))};

	saveBtn->clicked().connect([=]()
	{
		t->updateModel(model.get());

		if (model->validate())
		{
			model->saveData();
			LmsApp->notifyMsg(MsgType::Success, Wt::WString::tr("Lms.Settings.settings-saved"));
		}
		t->updateView(model.get());
	});

	discardBtn->clicked().connect([=]()
	{
		model->loadData();
		model->validate();
		t->updateView(model.get());
	});

	t->updateView(model.get());
}

} // namespace UserInterface


