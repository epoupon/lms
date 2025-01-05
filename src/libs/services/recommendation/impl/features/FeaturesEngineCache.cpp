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

#include "FeaturesEngineCache.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Service.hpp"

namespace lms::recommendation
{
    namespace
    {
        std::filesystem::path getCacheDirectory()
        {
            return core::Service<core::IConfig>::get()->getPath("working-dir", "/var/lms") / "cache" / "features";
        }

        std::filesystem::path getCacheNetworkFilePath()
        {
            return getCacheDirectory() / "network";
        }

        std::filesystem::path getCacheTrackPositionsFilePath()
        {
            return getCacheDirectory() / "track_positions";
        }

        bool networkToCacheFile(const som::Network& network, std::filesystem::path path)
        {
            try
            {
                boost::property_tree::ptree root;

                root.put("width", network.getWidth());
                root.put("height", network.getHeight());
                root.put("dim_count", network.getInputDimCount());

                for (som::InputVector::value_type weight : network.getDataWeights())
                    root.add("weights.weight", weight);

                for (som::Coordinate x = 0; x < network.getWidth(); ++x)
                {
                    for (som::Coordinate y = 0; y < network.getWidth(); ++y)
                    {
                        const auto& refVector = network.getRefVector({ x, y });

                        boost::property_tree::ptree node;
                        for (const auto& value : refVector)
                            node.add("values.value", value);

                        node.put("coord_x", x);
                        node.put("coord_y", y);

                        root.add_child("ref_vectors.ref_vector", node);
                    }
                }

                boost::property_tree::write_xml(path.string(), root);

                LMS_LOG(RECOMMENDATION, DEBUG, "Created network cache");
                return true;
            }
            catch (boost::property_tree::ptree_error& error)
            {
                LMS_LOG(RECOMMENDATION, ERROR, "Cannot create network cache: " << error.what());
                return false;
            }
        }
    } // namespace

    std::optional<som::Network> FeaturesEngineCache::createNetworkFromCacheFile(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path))
            return std::nullopt;

        try
        {
            LMS_LOG(RECOMMENDATION, INFO, "Reading network from cache...");

            boost::property_tree::ptree root;

            boost::property_tree::read_xml(path.string(), root);

            som::Coordinate width{ root.get<som::Coordinate>("width") };
            som::Coordinate height{ root.get<som::Coordinate>("height") };
            std::size_t dimCount{ root.get<std::size_t>("dim_count") };

            som::Network res{ width, height, dimCount };

            {
                som::InputVector weights{ dimCount };
                std::size_t i{};
                for (const auto& val : root.get_child("weights"))
                    weights[i++] = val.second.get_value<double>();

                res.setDataWeights(weights);
            }

            for (const auto& node : root.get_child("ref_vectors"))
            {
                som::Coordinate x{ node.second.get<som::Coordinate>("coord_x") };
                som::Coordinate y{ node.second.get<som::Coordinate>("coord_y") };

                som::InputVector refVector{ dimCount };
                std::size_t i{};
                for (const auto& val : node.second.get_child("values"))
                    refVector[i++] = val.second.get_value<som::InputVector::value_type>();

                res.setRefVector({ x, y }, refVector);
            }

            LMS_LOG(RECOMMENDATION, INFO, "Successfully read network from cache");

            return res;
        }
        catch (boost::property_tree::ptree_error& error)
        {
            LMS_LOG(RECOMMENDATION, ERROR, "Cannot read network cache: " << error.what());
            return std::nullopt;
        }
    }

    bool FeaturesEngineCache::objectPositionToCacheFile(const TrackPositions& trackPositions, const std::filesystem::path& path)
    {
        try
        {
            boost::property_tree::ptree root;

            for (const auto& [id, positions] : trackPositions)
            {
                boost::property_tree::ptree node;

                node.put("id", id.getValue());

                for (const som::Position& position : positions)
                {
                    boost::property_tree::ptree positionNode;
                    positionNode.put("x", position.x);
                    positionNode.put("y", position.y);

                    node.add_child("position.position", positionNode);
                }

                root.add_child("objects.object", node);
            }

            boost::property_tree::write_xml(path.string(), root);
            return true;
        }
        catch (boost::property_tree::ptree_error& error)
        {
            LMS_LOG(RECOMMENDATION, ERROR, "Cannot cache object position: " << error.what());
            return false;
        }
    }

    std::optional<FeaturesEngineCache::TrackPositions> FeaturesEngineCache::createObjectPositionsFromCacheFile(const std::filesystem::path& path)
    {
        try
        {
            LMS_LOG(RECOMMENDATION, INFO, "Reading object position from cache...");

            boost::property_tree::ptree root;

            boost::property_tree::read_xml(path.string(), root);

            TrackPositions res;

            for (const auto& object : root.get_child("objects"))
            {
                const db::TrackId id{ object.second.get<db::IdType::ValueType>("id") };
                for (const auto& position : object.second.get_child("position"))
                {
                    auto x = position.second.get<som::Coordinate>("x");
                    auto y = position.second.get<som::Coordinate>("y");

                    res[id].push_back({ x, y });
                }
            }

            LMS_LOG(RECOMMENDATION, INFO, "Successfully read object position from cache");

            return res;
        }
        catch (boost::property_tree::ptree_error& error)
        {
            LMS_LOG(RECOMMENDATION, ERROR, "Cannot create object position from cache file: " << error.what());
            return std::nullopt;
        }
    }

    void FeaturesEngineCache::invalidate()
    {
        std::filesystem::remove(getCacheNetworkFilePath());
        std::filesystem::remove(getCacheTrackPositionsFilePath());
    }

    std::optional<FeaturesEngineCache> FeaturesEngineCache::read()
    {
        auto network{ createNetworkFromCacheFile(getCacheNetworkFilePath()) };
        if (!network)
            return std::nullopt;

        auto trackPositions{ createObjectPositionsFromCacheFile(getCacheTrackPositionsFilePath()) };
        if (!trackPositions)
            return std::nullopt;

        return FeaturesEngineCache{ std::move(*network), std::move(*trackPositions) };
    }

    void FeaturesEngineCache::write() const
    {
        std::filesystem::create_directories(core::Service<core::IConfig>::get()->getPath("working-dir", "/var/lms") / "cache" / "features");

        if (!networkToCacheFile(_network, getCacheNetworkFilePath())
            || !objectPositionToCacheFile(_trackPositions, getCacheTrackPositionsFilePath()))
        {
            invalidate();
        }
    }

    FeaturesEngineCache::FeaturesEngineCache(som::Network network, TrackPositions trackPositions)
        : _network{ std::move(network) }
        , _trackPositions{ std::move(trackPositions) }
    {
    }

} // namespace lms::recommendation
