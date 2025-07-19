/*
 * Copyright (C) 2024 Emeric Poupon
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

#include "database/objects/TrackLyrics.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/PathTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::TrackLyrics)

namespace lms::db
{
    namespace
    {
        Wt::Dbo::Query<Wt::Dbo::ptr<TrackLyrics>> createQuery(Session& session, const TrackLyrics::FindParameters& params)
        {
            auto query{ session.getDboSession()->query<Wt::Dbo::ptr<TrackLyrics>>("SELECT t_lrc from track_lyrics t_lrc") };

            if (params.track.isValid())
                query.where("t_lrc.track_id = ?").bind(params.track);

            if (params.external.has_value())
                query.where("t_lrc.absolute_file_path " + std::string{ *params.external ? "<>" : "=" } + " ''");

            switch (params.sortMethod)
            {
            case TrackLyricsSortMethod::None:
                break;
            case TrackLyricsSortMethod::ExternalFirst:
                query.orderBy("CASE WHEN absolute_file_path <> '' THEN 0 ELSE 1 END");
                break;
            case TrackLyricsSortMethod::EmbeddedFirst:
                query.orderBy("CASE WHEN absolute_file_path = '' THEN 0 ELSE 1 END");
                break;
            }

            return query;
        }
    } // namespace

    TrackLyrics::pointer TrackLyrics::create(Session& session)
    {
        return session.getDboSession()->add(std::unique_ptr<TrackLyrics>{ new TrackLyrics{} });
    }

    std::size_t TrackLyrics::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM track_lyrics"));
    }

    std::size_t TrackLyrics::getExternalLyricsCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(t_lrc.id) FROM track_lyrics t_lrc").where("t_lrc.absolute_file_path <> ''"));
    }

    TrackLyrics::pointer TrackLyrics::find(Session& session, TrackLyricsId id)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<TrackLyrics>>("SELECT t_lrc from track_lyrics t_lrc").where("t_lrc.id = ?").bind(id));
    }

    TrackLyrics::pointer TrackLyrics::find(Session& session, const std::filesystem::path& path)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<TrackLyrics>>("SELECT t_lrc from track_lyrics t_lrc").where("t_lrc.absolute_file_path = ?").bind(path));
    }

    void TrackLyrics::find(Session& session, const FindParameters& params, const std::function<void(const TrackLyrics::pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ createQuery(session, params) };
        utils::forEachQueryRangeResult(query, params.range, func);
    }

    void TrackLyrics::find(Session& session, TrackLyricsId& lastRetrievedId, std::size_t count, const std::function<void(const TrackLyrics::pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<TrackLyrics>>("SELECT t_lrc from track_lyrics t_lrc").orderBy("t_lrc.id").where("t_lrc.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        utils::forEachQueryResult(query, [&](const TrackLyrics::pointer& lyrics) {
            func(lyrics);
            lastRetrievedId = lyrics->getId();
        });
    }

    RangeResults<TrackLyricsId> TrackLyrics::findOrphanIds(Session& session, std::optional<Range> range)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<TrackLyricsId>("select t_lrc.id from track_lyrics t_lrc") };
        query.leftJoin("track t ON t_lrc.track_id = t.id");
        query.where("t.id IS NULL");

        return utils::execRangeQuery<TrackLyricsId>(query, range);
    }

    void TrackLyrics::findAbsoluteFilePath(Session& session, TrackLyricsId& lastRetrievedId, std::size_t count, const std::function<void(TrackLyricsId trackLyricsId, const std::filesystem::path& absoluteFilePath)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<std::tuple<TrackLyricsId, std::filesystem::path>>("SELECT t_lrc.id,t_lrc.absolute_file_path from track_lyrics t_lrc").orderBy("t_lrc.id").where("t_lrc.id > ?").bind(lastRetrievedId).limit(static_cast<int>(count)) };

        utils::forEachQueryResult(query, [&](const auto& res) {
            func(std::get<0>(res), std::get<1>(res));
            lastRetrievedId = std::get<0>(res);
        });
    }

    TrackLyrics::SynchronizedLines TrackLyrics::getSynchronizedLines() const
    {
        SynchronizedLines synchronizedLines;
        assert(_synchronized);

        {
            Wt::Json::Object root;
            Wt::Json::parse(_lines, root);

            assert(root.type("lines") == Wt::Json::Type::Array);
            const Wt::Json::Array& linesArray = root.get("lines");
            for (const Wt::Json::Value& line : linesArray)
            {
                assert(line.type() == Wt::Json::Type::Object);
                const Wt::Json::Object& entry = line;
                std::chrono::milliseconds timestamp{ entry.get("timestamp") };
                std::string lineText{ static_cast<std::string>(entry.get("value")) };
                synchronizedLines.emplace(timestamp, std::move(lineText));
            }
        }

        return synchronizedLines;
    }

    std::vector<std::string> TrackLyrics::getUnsynchronizedLines() const
    {
        std::vector<std::string> unsynchronizedLines;
        assert(!_synchronized);

        {
            Wt::Json::Object root;
            Wt::Json::parse(_lines, root);

            assert(root.type("lines") == Wt::Json::Type::Array);
            const Wt::Json::Array& linesArray = root.get("lines");
            for (const Wt::Json::Value& line : linesArray)
                unsynchronizedLines.push_back(line.toString());
        }

        return unsynchronizedLines;
    }

    void TrackLyrics::setAbsoluteFilePath(const std::filesystem::path& p)
    {
        assert(p.is_absolute());
        _fileAbsolutePath = p;
        _fileStem = p.stem().string();
    }

    void TrackLyrics::setSynchronizedLines(const SynchronizedLines& synchronizedLines)
    {
        Wt::Json::Object root;

        Wt::Json::Array lines;
        for (const auto& [timestamp, lineText] : synchronizedLines)
        {
            Wt::Json::Object line;
            line["timestamp"] = Wt::Json::Value{ timestamp.count() };
            line["value"] = Wt::Json::Value{ lineText };

            lines.push_back(std::move(line));
        }

        root["lines"] = std::move(lines);

        _synchronized = true;
        _lines = Wt::Json::serialize(root);
    }

    void TrackLyrics::setUnsynchronizedLines(std::span<const std::string> unsynchronizedLines)
    {
        Wt::Json::Object root;

        Wt::Json::Array lines;
        for (const auto& lineText : unsynchronizedLines)
            lines.push_back(Wt::Json::Value{ lineText });

        root["lines"] = std::move(lines);

        _synchronized = false;
        _lines = Wt::Json::serialize(root);
    }

} // namespace lms::db
