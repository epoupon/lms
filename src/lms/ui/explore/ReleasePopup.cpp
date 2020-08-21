/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "ReleasePopup.hpp"

#include "LmsApplication.hpp"

namespace UserInterface
{

	void
	displayReleasePopupMenu(Wt::WInteractWidget& target,
			Database::IdType releaseId,
			PlayQueueActionSignal& releasesAction)
	{
			Wt::WPopupMenu* popup {LmsApp->createPopupMenu()};

			popup->addItem(Wt::WString::tr("Lms.Explore.play-shuffled"))
				->triggered().connect(&target, [&releasesAction, releaseId]
					{
						releasesAction.emit(PlayQueueAction::PlayShuffled, {releaseId});
					});
			popup->addItem(Wt::WString::tr("Lms.Explore.play-last"))
				->triggered().connect(&target, [&releasesAction, releaseId]
					{
						releasesAction.emit(PlayQueueAction::PlayLast, {releaseId});
					});

			popup->popup(&target);
	}

} // namespace UserInterface

