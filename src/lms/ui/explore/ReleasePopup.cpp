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

#include <Wt/WPopupMenu.h>

#include "lmscore/database/Release.hpp"
#include "lmscore/database/Session.hpp"
#include "lmscore/database/User.hpp"
#include "resource/DownloadResource.hpp"
#include "LmsApplication.hpp"

namespace UserInterface
{

	void
	displayReleasePopupMenu(Wt::WInteractWidget& target,
			Database::ReleaseId releaseId,
			PlayQueueActionReleaseSignal& releasesAction)
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

			bool isStarred {};
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				if (auto release {Database::Release::getById(LmsApp->getDbSession(), releaseId)})
					isStarred = LmsApp->getUser()->hasStarredRelease(release);
			}

			popup->addItem(Wt::WString::tr(isStarred ? "Lms.Explore.unstar" : "Lms.Explore.star"))
				->triggered().connect(&target, [=]
					{
						auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

						auto release {Database::Release::getById(LmsApp->getDbSession(), releaseId)};
						if (!release)
							return;

						if (isStarred)
							LmsApp->getUser().modify()->unstarRelease(release);
						else
							LmsApp->getUser().modify()->starRelease(release);
					});
			popup->addItem(Wt::WString::tr("Lms.Explore.download"))
				->setLink(Wt::WLink {std::make_unique<DownloadReleaseResource>(releaseId)});

			popup->popup(&target);
	}

} // namespace UserInterface

