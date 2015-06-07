/*
 * Copyright (C) 2013 Emeric Poupon
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

#ifndef UI_SETTINGS_ACCOUNT_FORM_VIEW_HPP
#define UI_SETTINGS_ACCOUNT_FORM_VIEW_HPP

#include <Wt/WContainerWidget>
#include <Wt/WTemplateFormView>
#include <Wt/WText>

namespace UserInterface {
namespace Settings {

class AccountFormModel;

class AccountFormView : public Wt::WTemplateFormView
{
	public:
		AccountFormView(std::string userId, Wt::WContainerWidget *parent = 0);

	private:
		void processCancel();	// reload from DB
		void processSave(); 	// commit into DB

		AccountFormModel*	_model;
		Wt::WText*		_applyInfo;

};

} // namespace Settings
} // namespace UserInterface

#endif
