/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "ClientInfo.hpp"

#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>

#include "core/String.hpp"

#include "SubsonicResponse.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        Limitation::Type parseLimitationType(std::string_view str)
        {
            if (str == "audioChannels")
                return Limitation::Type::AudioChannels;
            if (str == "audioBitrate")
                return Limitation::Type::AudioBitrate;
            if (str == "audioProfile")
                return Limitation::Type::AudioProfile;
            if (str == "audioSamplerate")
                return Limitation::Type::AudioSamplerate;
            if (str == "audioBitdepth")
                return Limitation::Type::AudioBitdepth;

            throw BadParameterGenericError{ "ClientInfo::CodecProfile::name", "unexpected value '" + std::string{ str } + "'" };
        }

        Limitation::ComparisonOperator parseComparisonOperator(std::string_view str)
        {
            // Equals, NotEquals, LessThanEqual, GreaterThanEqual, EqualsAny, or NotEqualsAny
            if (str == "Equals")
                return Limitation::ComparisonOperator::Equals;
            if (str == "NotEquals")
                return Limitation::ComparisonOperator::NotEquals;
            if (str == "LessThanEqual")
                return Limitation::ComparisonOperator::LessThanEqual;
            if (str == "GreaterThanEqual")
                return Limitation::ComparisonOperator::GreaterThanEqual;
            if (str == "EqualsAny")
                return Limitation::ComparisonOperator::EqualsAny;
            if (str == "NotEqualsAny")
                return Limitation::ComparisonOperator::NotEqualsAny;

            throw BadParameterGenericError{ "ClientInfo::CodecProfile::comparison", "unexpected value '" + std::string{ str } + "'" };
        }

        std::vector<std::string> parseValues(std::string_view str)
        {
            std::vector<std::string> res;
            for (std::string_view v : core::stringUtils::splitString(str, '|'))
                res.emplace_back(v);
            return res;
        }

        void checkLimitationValidity(const Limitation& limitation)
        {
            if (limitation.values.empty())
                throw BadParameterGenericError{ "ClientInfo::CodecProfile::value", "must have at least one value" };
            if (limitation.values.size() > 1 && (limitation.comparison != Limitation::ComparisonOperator::EqualsAny && limitation.comparison != Limitation::ComparisonOperator::NotEqualsAny))
                throw BadParameterGenericError{ "ClientInfo::CodecProfile::value", "multiple values must use EqualsAny or NotEqualsAny comparison operator" };

            // only numeric values are allowed for some limitation types
            switch (limitation.name)
            {
            case Limitation::Type::AudioChannels:
            case Limitation::Type::AudioBitrate:
            case Limitation::Type::AudioSamplerate:
            case Limitation::Type::AudioBitdepth:
                {
                    // must be a numeric value
                    std::string_view value{ limitation.values.front() };
                    if (!core::stringUtils::readAs<unsigned>(value))
                        throw BadParameterGenericError{ "ClientInfo::CodecProfile::value", "value'" + std::string{ value } + "' is not a number" };
                }
                break;

            case Limitation::Type::AudioProfile:
                // any value is allowed
                break;
            }
        }
    } // namespace

    ClientInfo parseClientInfoFromJson(std::istream& is)
    {
        ClientInfo res;

        const std::string msgBody{ std::istreambuf_iterator<char>{ is }, std::istreambuf_iterator<char>{} };

        try
        {
            Wt::Json::Object root;
            Wt::Json::parse(msgBody, root);

            res.name = static_cast<std::string>(root.get("name"));
            res.platform = static_cast<std::string>(root.get("platform"));
            if (const Wt::Json::Value & maxAudioBitRate{ root.get("maxAudioBitrate") }; maxAudioBitRate.type() != Wt::Json::Type::Null)
                res.maxAudioBitrate = static_cast<long>(maxAudioBitRate);
            if (const Wt::Json::Value & maxTranscodingAudioBitrate{ root.get("maxTranscodingAudioBitrate") }; maxTranscodingAudioBitrate.type() != Wt::Json::Type::Null)
                res.maxTranscodingAudioBitrate = static_cast<long>(maxTranscodingAudioBitrate);

            if (const Wt::Json::Value & directPlayProfiles{ root.get("directPlayProfiles") }; directPlayProfiles.type() != Wt::Json::Type::Null)
            {
                for (const Wt::Json::Object& profile : static_cast<const Wt::Json::Array&>(directPlayProfiles))
                {
                    DirectPlayProfile directPlayProfile;
                    directPlayProfile.container = static_cast<std::string>(profile.get("container"));
                    if (directPlayProfile.container.empty())
                        throw BadParameterGenericError{ "ClientInfo::DirectPlayProfile::container", "cannot be empty" };

                    for (std::string_view codec : core::stringUtils::splitString(static_cast<std::string>(profile.get("audioCodec")), ','))
                        directPlayProfile.audioCodecs.push_back(std::string{ codec });

                    if (directPlayProfile.audioCodecs.empty())
                        throw BadParameterGenericError{ "ClientInfo::DirectPlayProfile::audioCodec", "cannot be empty" };

                    directPlayProfile.protocol = static_cast<std::string>(profile.get("protocol"));
                    if (directPlayProfile.protocol.empty())
                        throw BadParameterGenericError{ "ClientInfo::DirectPlayProfile::protocol", "cannot be empty" };

                    if (const Wt::Json::Value maxAudioChannels{ profile.get("maxAudioChannels") }; maxAudioChannels.type() != Wt::Json::Type::Null)
                        directPlayProfile.maxAudioChannels = static_cast<long>(maxAudioChannels);

                    res.directPlayProfiles.push_back(directPlayProfile);
                }
            }

            if (const Wt::Json::Value & transcodingProfiles{ root.get("transcodingProfiles") }; transcodingProfiles.type() != Wt::Json::Type::Null)
            {
                for (const Wt::Json::Object& profile : static_cast<const Wt::Json::Array&>(transcodingProfiles))
                {
                    TranscodingProfile transcodingProfile;
                    transcodingProfile.container = static_cast<std::string>(profile.get("container"));
                    if (transcodingProfile.container.empty())
                        throw BadParameterGenericError{ "ClientInfo::TranscodingProfile::container", "cannot be empty" };

                    transcodingProfile.audioCodec = static_cast<std::string>(profile.get("audioCodec"));
                    if (transcodingProfile.audioCodec.empty())
                        throw BadParameterGenericError{ "ClientInfo::TranscodingProfile::audioCodec", "cannot be empty" };

                    transcodingProfile.protocol = static_cast<std::string>(profile.get("protocol"));
                    if (transcodingProfile.protocol.empty())
                        throw BadParameterGenericError{ "ClientInfo::TranscodingProfile::protocol", "cannot be empty" };

                    if (const Wt::Json::Value maxAudioChannels{ profile.get("maxAudioChannels") }; maxAudioChannels.type() != Wt::Json::Type::Null)
                        transcodingProfile.maxAudioChannels = static_cast<long>(maxAudioChannels);

                    res.transcodingProfiles.push_back(transcodingProfile);
                }
            }

            // codecProfiles
            if (const Wt::Json::Value & codecProfiles{ root.get("codecProfiles") }; codecProfiles.type() != Wt::Json::Type::Null)
            {
                for (const Wt::Json::Object& profile : static_cast<const Wt::Json::Array&>(codecProfiles))
                {
                    CodecProfile codecProfile;
                    codecProfile.type = static_cast<std::string>(profile.get("type"));
                    if (codecProfile.type != "AudioCodec" && codecProfile.type != "VideoCodec")
                        throw BadParameterGenericError{ "ClientInfo::CodecProfile::type", "unexpected value '" + codecProfile.type + "'" };

                    codecProfile.name = static_cast<std::string>(profile.get("name"));
                    if (codecProfile.name.empty())
                        throw BadParameterGenericError{ "ClientInfo::CodecProfile::name", "name must not be empty" };

                    if (const Wt::Json::Value & limitations{ profile.get("limitations") }; limitations.type() != Wt::Json::Type::Null)
                    {
                        for (const Wt::Json::Object& limitation : static_cast<const Wt::Json::Array&>(limitations))
                        {
                            Limitation lim;
                            lim.name = parseLimitationType(static_cast<std::string>(limitation.get("name")));
                            lim.comparison = parseComparisonOperator(static_cast<std::string>(limitation.get("comparison")));
                            lim.values = parseValues(static_cast<std::string>(limitation.get("value")));
                            lim.required = static_cast<bool>(limitation.get("required"));

                            checkLimitationValidity(lim);
                            codecProfile.limitations.push_back(lim);
                        }
                    }

                    res.codecProfiles.push_back(codecProfile);
                }
            }
        }
        catch (const Wt::WException& error)
        {
            throw BadParameterGenericError{ "ClientInfo", error.what() };
        }

        return res;
    }
} // namespace lms::api::subsonic