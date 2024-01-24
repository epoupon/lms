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

#include "database/MediaLibrary.hpp"
#include "database/Session.hpp"
#include "utils/String.hpp"

namespace Database
{
    void ScanSettings::init(Session& session)
    {
        session.checkWriteTransaction();

        if (pointer settings{ get(session) })
            return;

        session.getDboSession().add(std::make_unique<ScanSettings>());
    }

    ScanSettings::pointer ScanSettings::get(Session& session)
    {
        session.checkReadTransaction();

        return session.getDboSession().find<ScanSettings>().resultValue();
    }

    std::vector<std::filesystem::path> ScanSettings::getAudioFileExtensions() const
    {
        const auto extensions{ StringUtils::splitString(_audioFileExtensions, " ") };

        std::vector<std::filesystem::path> res(std::cbegin(extensions), std::cend(extensions));
        std::sort(std::begin(res), std::end(res));
        res.erase(std::unique(std::begin(res), std::end(res)), std::end(res));

        return res;
    }

    void ScanSettings::addAudioFileExtension(const std::filesystem::path& ext)
    {
        _audioFileExtensions += " " + ext.string();
    }

    std::vector<std::string_view> ScanSettings::getExtraTagsToScan() const
    {
        return StringUtils::splitString(_extraTagsToScan, ";");
    }

    void ScanSettings::setExtraTagsToScan(const std::vector<std::string_view>& extraTags)
    {
        std::string newTagsToScan{ StringUtils::joinStrings(extraTags, ";") };
        if (newTagsToScan != _extraTagsToScan)
            incScanVersion();

        _extraTagsToScan = std::move(newTagsToScan);
    }

    void ScanSettings::incScanVersion()
    {
        _scanVersion += 1;
    }
} // namespace Database
