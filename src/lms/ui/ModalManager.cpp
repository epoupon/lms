/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "ModalManager.hpp"
#include "utils/Logger.hpp"

namespace UserInterface
{
	ModalManager::ModalManager()
		: _closed {this, "closed"}
	{
		_closed.connect([=](const std::string& id)
		{
			LMS_LOG(UI, DEBUG) << "Received closed for id '" << id << "'";
			for (int i {}; i < count(); ++i)
			{
				Wt::WWidget* widget {this->widget(i)};
				LMS_LOG(UI, DEBUG) << "Widget " << i << ", id = '" << widget->id();
				if (widget->id() == id)
				{
					removeWidget(widget);
					break;
				}
			}
		});
	}

	void
	ModalManager::show(std::unique_ptr<Wt::WWidget> modalWidget)
	{
		LMS_LOG(UI, DEBUG) << "Want to show, id = " << modalWidget->id();

		std::ostringstream oss;
		oss
			<< R"({const modalElement = )" << jsRef() << R"(.getElementsByClassName('modal')[0];)"
			<< R"(const modal = bootstrap.Modal.getOrCreateInstance(modalElement);)"
			<< R"(modal.show();)"
			<< R"(modalElement.addEventListener('hidden.bs.modal', function () {)"
				<< _closed.createCall({"'" + modalWidget->id() + "'"})
				<< R"(modal.dispose();)"
			<< R"(});})";

		LMS_LOG(UI, DEBUG) << "Running JS '" << oss.str() << "'";

		doJavaScript(oss.str());

		addWidget(std::move(modalWidget));
	}

	void
	ModalManager::dispose(Wt::WWidget* modalWidget)
	{
		std::ostringstream oss;
		oss
			<< R"({const modalElementParent = document.getElementById(')" << modalWidget->id() << R"(');)"
			<< R"(const modalElement = modalElementParent.getElementsByClassName('modal')[0];)"
			<< R"(const modal = bootstrap.Modal.getInstance(modalElement);)"
			<< R"(modal.hide();)"
			<< R"(})";

		LMS_LOG(UI, DEBUG) << "Running JS '" << oss.str() << "'";

		doJavaScript(oss.str());
	}
}
