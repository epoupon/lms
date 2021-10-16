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

#include "TrackPopup.hpp"

#include <Wt/WPopupMenu.h>

#include "lmscore/database/Session.hpp"
#include "lmscore/database/Track.hpp"
#include "lmscore/database/User.hpp"
#include "resource/DownloadResource.hpp"
#include "LmsApplication.hpp"

namespace UserInterface
{

	void
	displayTrackPopupMenu(Wt::WInteractWidget& target,
			Database::TrackId trackId,
			PlayQueueActionTrackSignal& tracksAction)
	{
			Wt::WPopupMenu* popup {LmsApp->createPopupMenu()};

			popup->addItem(Wt::WString::tr("Lms.Explore.play-last"))
				->triggered().connect(&target, [=, &tracksAction]
				{
					tracksAction.emit(PlayQueueAction::PlayLast, {trackId});
				});

			bool isStarred {};
			{
				auto transaction {LmsApp->getDbSession().createSharedTransaction()};

				if (auto track {Database::Track::getById(LmsApp->getDbSession(), trackId)})
					isStarred = LmsApp->getUser()->hasStarredTrack(track);
			}
			popup->addItem(Wt::WString::tr(isStarred ? "Lms.Explore.unstar" : "Lms.Explore.star"))
				->triggered().connect(&target, [=]
					{
						auto transaction {LmsApp->getDbSession().createUniqueTransaction()};

						auto track {Database::Track::getById(LmsApp->getDbSession(), trackId)};
						if (!track)
							return;

						if (isStarred)
							LmsApp->getUser().modify()->unstarTrack(track);
						else
							LmsApp->getUser().modify()->starTrack(track);
					});
			popup->addItem(Wt::WString::tr("Lms.Explore.download"))
				->setLink(Wt::WLink {std::make_unique<DownloadTrackResource>(trackId)});

			popup->popup(&target);
	}

} // namespace UserInterface

