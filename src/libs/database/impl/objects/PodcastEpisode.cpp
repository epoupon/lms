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

#include "database/objects/PodcastEpisode.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "database/Session.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/Podcast.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PathTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::PodcastEpisode)

namespace lms::db
{
    namespace
    {
        Wt::Dbo::Query<Wt::Dbo::ptr<PodcastEpisode>> createQuery(Session& session, const PodcastEpisode::FindParameters& params)
        {
            auto query{ session.getDboSession()->query<Wt::Dbo::ptr<PodcastEpisode>>("SELECT p_e from podcast_episode p_e") };

            if (params.manualDownloadState.has_value())
                query.where("p_e.manual_download_state = ?").bind(static_cast<int>(params.manualDownloadState.value()));

            if (params.podcast.isValid())
                query.where("p_e.podcast_id = ?").bind(params.podcast);

            switch (params.sortMode)
            {
            case PodcastEpisodeSortMode::None:
                break;
            case PodcastEpisodeSortMode::PubDateAsc:
                query.orderBy("p_e.pub_date ASC");
                break;
            case PodcastEpisodeSortMode::PubDateDesc:
                query.orderBy("p_e.pub_date DESC");
                break;
            }
            return query;
        }
    } // namespace

    PodcastEpisode::PodcastEpisode(ObjectPtr<Podcast> podcast)
        : _podcast{ getDboPtr(podcast) }
    {
    }

    PodcastEpisode::pointer PodcastEpisode::create(Session& session, ObjectPtr<Podcast> podcast)
    {
        return session.getDboSession()->add(std::unique_ptr<PodcastEpisode>{ new PodcastEpisode{ podcast } });
    }

    std::size_t PodcastEpisode::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM podcast_episode"));
    }

    PodcastEpisode::pointer PodcastEpisode::find(Session& session, PodcastEpisodeId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<PodcastEpisode>>("SELECT p_e from podcast_episode p_e").where("p_e.id = ?").bind(id));
    }

    PodcastEpisode::pointer PodcastEpisode::findNewtestEpisode(Session& session, PodcastId podcastId)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<PodcastEpisode>>("SELECT p_e from podcast_episode p_e").where("p_e.podcast_id = ?").bind(podcastId).orderBy("p_e.pub_date DESC").limit(1));
    }

    void PodcastEpisode::find(Session& session, const FindParameters& params, std::function<void(const pointer&)> func)
    {
        session.checkReadTransaction();

        auto query{ createQuery(session, params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    ObjectPtr<Artwork> PodcastEpisode::getArtwork() const
    {
        return _artwork;
    }

    ArtworkId PodcastEpisode::getArtworkId() const
    {
        return _artwork.id();
    }

    void PodcastEpisode::setArtwork(ObjectPtr<Artwork> artwork)
    {
        _artwork = getDboPtr(artwork);
    }
} // namespace lms::db
