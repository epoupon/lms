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

#include "database/objects/ScanSettings.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "core/String.hpp"

#include "DboAccess.hpp"
#include "ScanSettingsDbo.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::ScanSettings)

namespace lms::db
{
    ScanSettings::ScanSettings(std::string_view name)
        : _name{ name }
    {
    }

    ScanSettings ScanSettings::create(ISession& session, std::string_view name)
    {
        return utils::addObject(session, std::unique_ptr<dbo::ScanSettings>(new dbo::ScanSettings{ name }));
    }

    void ScanSettings::destroy(ISession& session, std::span<const ScanSettingsId> ids)
    {
        utils::destroyObjects<dbo::ScanSettings>(session, ids);
    }

    std::optional<ScanSettings> ScanSettings::find(ISession& session, ScanSettingsId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::ScanSettings>>("SELECT s_s from scan_settings s_s").where("s_s.id = ?").bind(id));
    }

    std::optional<ScanSettings> ScanSettings::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::ScanSettings>().where("name = ?").bind(name));
    }

    std::vector<std::string_view> ScanSettings::getExtraTagsToScan() const
    {
        std::vector<std::string_view> tags{ core::stringUtils::splitString(_extraTagsToScan, ';') };
        if (tags.size() == 1 && tags.front().empty())
            tags.clear();

        return tags;
    }

    std::vector<std::string> ScanSettings::getArtistTagDelimiters() const
    {
        return core::stringUtils::splitEscapedStrings(_artistTagDelimiters, ';', '\\');
    }

    std::vector<std::string> ScanSettings::getDefaultTagDelimiters() const
    {
        return core::stringUtils::splitEscapedStrings(_defaultTagDelimiters, ';', '\\');
    }

    std::vector<std::string> ScanSettings::getArtistsToNotSplit() const
    {
        return core::stringUtils::splitEscapedStrings(_artistsToNotSplit, ';', '\\');
    }

    void ScanSettings::setExtraTagsToScan(std::span<const std::string_view> extraTags)
    {
        std::string newTagsToScan{ core::stringUtils::joinStrings(extraTags, ";") };
        if (newTagsToScan != _extraTagsToScan)
        {
            _extraTagsToScan.swap(newTagsToScan);
            incAudioScanVersion();
        }
    }

    void ScanSettings::setArtistTagDelimiters(std::span<const std::string_view> delimiters)
    {
        std::string tagDelimiters{ core::stringUtils::escapeAndJoinStrings(delimiters, ';', '\\') };
        if (tagDelimiters != _artistTagDelimiters)
        {
            _artistTagDelimiters.swap(tagDelimiters);
            incAudioScanVersion();
        }
    }

    void ScanSettings::setArtistsToNotSplit(std::span<const std::string_view> artists)
    {
        std::string artistsToNotSplit{ core::stringUtils::escapeAndJoinStrings(artists, ';', '\\') };
        if (artistsToNotSplit != _artistsToNotSplit)
        {
            _artistsToNotSplit.swap(artistsToNotSplit);
            incAudioScanVersion();
        }
    }

    void ScanSettings::setDefaultTagDelimiters(std::span<const std::string_view> delimiters)
    {
        std::string tagDelimiters{ core::stringUtils::escapeAndJoinStrings(delimiters, ';', '\\') };
        if (tagDelimiters != _defaultTagDelimiters)
        {
            _defaultTagDelimiters.swap(tagDelimiters);
            incAudioScanVersion();
        }
    }

    void ScanSettings::setSkipSingleReleasePlayLists(bool value)
    {
        _skipSingleReleasePlayLists = value;
    }

    void ScanSettings::setAllowMBIDArtistMerge(bool value)
    {
        _allowMBIDArtistMerge = value;
    }

    void ScanSettings::setArtistImageFallbackToRelease(bool value)
    {
        _artistImageFallbackToRelease = value;
    }

    void ScanSettings::incAudioScanVersion()
    {
        _audioScanVersion += 1;
    }

    void ScanSettings::update(ISession& session, const ScanSettings& scanSettings)
    {
        utils::updateObject<dbo::ScanSettings>(session, scanSettings);
    }
} // namespace lms::db
