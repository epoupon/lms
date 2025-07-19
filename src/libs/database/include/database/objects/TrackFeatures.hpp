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

#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Wt/Dbo/Field.h>

#include "database/IdType.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/TrackId.hpp"

LMS_DECLARE_IDTYPE(TrackFeaturesId)

namespace lms::db
{
    class Session;
    class Track;

    using FeatureName = std::string;
    using FeatureValues = std::vector<double>;
    using FeatureValuesMap = std::unordered_map<FeatureName, FeatureValues>;

    class TrackFeatures final : public Object<TrackFeatures, TrackFeaturesId>
    {
    public:
        TrackFeatures() = default;

        // Find utilities
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, TrackFeaturesId id);
        static pointer find(Session& session, TrackId trackId);
        static RangeResults<TrackFeaturesId> find(Session& session, std::optional<Range> range = std::nullopt);

        FeatureValues getFeatureValues(const FeatureName& feature) const;
        FeatureValuesMap getFeatureValuesMap(const std::unordered_set<FeatureName>& featureNames) const;

        // Accessors
        Wt::Dbo::ptr<Track> getTrack() const { return _track; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _data, "data");
            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        TrackFeatures(ObjectPtr<Track> track, const std::string& jsonEncodedFeatures);
        static pointer create(Session& session, ObjectPtr<Track> track, const std::string& jsonEncodedFeatures);

        std::string _data;
        Wt::Dbo::ptr<Track> _track;
    };

} // namespace lms::db
