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
#include "ArtistListHelpers.hpp"

#include <Wt/WAnchor.h>

#include "services/database/Artist.hpp"
#include "services/database/Session.hpp"
#include "services/database/TrackArtistLink.hpp"
#include "utils/EnumSet.hpp"
#include "LmsApplication.hpp"
#include "Utils.hpp"

namespace UserInterface::ArtistListHelpers
{
	std::unique_ptr<Wt::WTemplate>
	createEntry(const Database::ObjectPtr<Database::Artist>& artist)
	{
		auto res {std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.Artists.template.entry"))};
		res->bindWidget("name", Utils::createArtistAnchor(artist));

		return res;
	}

	std::unique_ptr<ArtistLinkTypesModel>
	createArtistLinkTypesModel()
	{
		using namespace Database;

		std::unique_ptr<ArtistLinkTypesModel> linkTypesModel {std::make_unique<ArtistLinkTypesModel>()};

		EnumSet<TrackArtistLinkType> usedLinkTypes;
		{
			auto transaction {LmsApp->getDbSession().createSharedTransaction()};
			usedLinkTypes = TrackArtistLink::findUsedTypes(LmsApp->getDbSession());
		}

		auto addTypeIfUsed {[&](TrackArtistLinkType linkType, std::string_view stringKey)
		{
			if (!usedLinkTypes.contains(linkType))
				return;

			linkTypesModel->add(Wt::WString::trn(std::string {stringKey}, 2), linkType);
		}};

		// add default one first (none)
		linkTypesModel->add(Wt::WString::tr("Lms.Explore.Artists.linktype-all"), std::nullopt);

		// TODO: sort by translated strings
		addTypeIfUsed(TrackArtistLinkType::Artist, "Lms.Explore.Artists.linktype-artist");
		addTypeIfUsed(TrackArtistLinkType::ReleaseArtist, "Lms.Explore.Artists.linktype-releaseartist");
		addTypeIfUsed(TrackArtistLinkType::Composer, "Lms.Explore.Artists.linktype-composer");
		addTypeIfUsed(TrackArtistLinkType::Conductor, "Lms.Explore.Artists.linktype-conductor");
		addTypeIfUsed(TrackArtistLinkType::Lyricist, "Lms.Explore.Artists.linktype-lyricist");
		addTypeIfUsed(TrackArtistLinkType::Mixer, "Lms.Explore.Artists.linktype-mixer");
		addTypeIfUsed(TrackArtistLinkType::Performer, "Lms.Explore.Artists.linktype-performer");
		addTypeIfUsed(TrackArtistLinkType::Producer, "Lms.Explore.Artists.linktype-producer");
		addTypeIfUsed(TrackArtistLinkType::Remixer, "Lms.Explore.Artists.linktype-remixer");

		return linkTypesModel;
	}
}

