/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "responses/Genre.hpp"

#include "database/objects/Cluster.hpp"

#include "RequestContext.hpp"

namespace lms::api::subsonic
{
    Response::Node createGenreNode(RequestContext& context, const db::Cluster::pointer& cluster)
    {
        Response::Node clusterNode;

        switch (context.responseFormat)
        {
        case ResponseFormat::json:
            clusterNode.setAttribute("value", cluster->getName());
            break;
        case ResponseFormat::xml:
            clusterNode.setValue(cluster->getName());
            break;
        }
        clusterNode.setAttribute("songCount", cluster->getTrackCount());
        clusterNode.setAttribute("albumCount", cluster->getReleasesCount());

        return clusterNode;
    }
} // namespace lms::api::subsonic