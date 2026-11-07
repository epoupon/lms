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

#include "database/objects/Artwork.hpp"
#include "database/objects/Podcast.hpp"

#include "Session.hpp"
#include "Utils.hpp"
#include "detail/Types.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PathTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::PodcastEpisode)

namespace lms::db
{
    namespace
    {
        Wt::Dbo::Query<Wt::Dbo::ptr<PodcastEpisode>> createQuery(ISession& session, const PodcastEpisode::FindParameters& params)
        {
            auto query{ getDboSession(session)->query<Wt::Dbo::ptr<PodcastEpisode>>("SELECT p_e from podcast_episode p_e") };

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

    PodcastEpisode::pointer PodcastEpisode::create(ISession& session, ObjectPtr<Podcast> podcast)
    {
        return utils::addObject(session, std::unique_ptr<PodcastEpisode>{ new PodcastEpisode{ podcast } });
    }

    std::size_t PodcastEpisode::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM podcast_episode"));
    }

    PodcastEpisode::pointer PodcastEpisode::find(ISession& session, PodcastEpisodeId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<PodcastEpisode>>("SELECT p_e from podcast_episode p_e").where("p_e.id = ?").bind(id));
    }

    PodcastEpisode::pointer PodcastEpisode::findNewtestEpisode(ISession& session, PodcastId podcastId)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<PodcastEpisode>>("SELECT p_e from podcast_episode p_e").where("p_e.podcast_id = ?").bind(podcastId).orderBy("p_e.pub_date DESC").limit(1));
    }

    void PodcastEpisode::find(ISession& session, const FindParameters& params, std::function<void(const pointer&)> func)
    {
        checkReadTransaction(session);

        auto query{ createQuery(session, params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    std::optional<core::media::Container> PodcastEpisode::getContainer() const
    {
        return detail::getMediaContainerType(_container);
    }

    std::optional<core::media::Codec> PodcastEpisode::getCodec() const
    {
        return detail::getMediaCodecType(_codec);
    }

    ObjectPtr<Artwork> PodcastEpisode::getArtwork() const
    {
        return _artwork;
    }

    ArtworkId PodcastEpisode::getArtworkId() const
    {
        return _artwork.id();
    }

    void PodcastEpisode::setContainer(core::media::Container container)
    {
        _container = detail::getDbContainer(container);
    }

    void PodcastEpisode::setCodec(core::media::Codec codec)
    {
        _codec = detail::getDbCodec(codec);
    }

    void PodcastEpisode::setArtwork(ObjectPtr<Artwork> artwork)
    {
        _artwork = getDboPtr(artwork);
    }
} // namespace lms::db
