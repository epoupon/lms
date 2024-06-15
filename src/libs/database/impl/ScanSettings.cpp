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

#include "database/ScanSettings.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "core/String.hpp"
#include "database/MediaLibrary.hpp"
#include "database/Session.hpp"

#include "Utils.hpp"

namespace lms::db
{
    void ScanSettings::init(Session& session)
    {
        session.checkWriteTransaction();

        if (pointer settings{ get(session) })
            return;

        session.getDboSession()->add(std::make_unique<ScanSettings>());
    }

    ScanSettings::pointer ScanSettings::get(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<ScanSettings>());
    }

    std::vector<std::filesystem::path> ScanSettings::getAudioFileExtensions() const
    {
        const auto extensions{ core::stringUtils::splitString(_audioFileExtensions, ' ') };

        std::vector<std::filesystem::path> res(std::cbegin(extensions), std::cend(extensions));
        std::sort(std::begin(res), std::end(res));
        res.erase(std::unique(std::begin(res), std::end(res)), std::end(res));

        return res;
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

    void ScanSettings::setExtraTagsToScan(std::span<const std::string_view> extraTags)
    {
        std::string newTagsToScan{ core::stringUtils::joinStrings(extraTags, ";") };
        if (newTagsToScan != _extraTagsToScan)
            incScanVersion();

        _extraTagsToScan = std::move(newTagsToScan);
    }

    void ScanSettings::setArtistTagDelimiters(std::span<const std::string_view> delimiters)
    {
        std::string tagDelimiters{ core::stringUtils::escapeAndJoinStrings(delimiters, ';', '\\') };
        if (tagDelimiters != _artistTagDelimiters)
        {
            _artistTagDelimiters.swap(tagDelimiters);
            incScanVersion();
        }
    }

    void ScanSettings::setDefaultTagDelimiters(std::span<const std::string_view> delimiters)
    {
        std::string tagDelimiters{ core::stringUtils::escapeAndJoinStrings(delimiters, ';', '\\') };
        if (tagDelimiters != _defaultTagDelimiters)
        {
            _defaultTagDelimiters.swap(tagDelimiters);
            incScanVersion();
        }
    }

    void ScanSettings::incScanVersion()
    {
        _scanVersion += 1;
    }
} // namespace lms::db
