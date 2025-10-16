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

#include <type_traits>

#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>

#include "core/String.hpp"

#include "SubsonicResponse.hpp"

namespace lms::api::subsonic
{
    namespace
    {
        template<typename T>
        std::optional<T> parseValue(const Wt::Json::Object& object, const std::string& entry)
        {
            try
            {
                std::optional<T> res;

                const Wt::Json::Value& value{ object.get(entry) };
                if (value.isNull())
                    return res;

                if constexpr (std::is_same_v<T, bool>)
                {
                    if (value.type() != Wt::Json::Type::Bool)
                        throw BadParameterGenericError{ entry, "field must be a boolean" };
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    if (value.type() != Wt::Json::Type::String)
                        throw BadParameterGenericError{ entry, "field must be a string" };
                }
                else if constexpr (std::is_integral_v<T>)
                {
                    if (value.type() != Wt::Json::Type::Number)
                        throw BadParameterGenericError{ entry, "field must be a number" };
                }
                else
                {
                    static_assert(false, "Unhandled type");
                }

                return static_cast<T>(value);
            }
            catch (const Wt::WException& e)
            {
                throw BadParameterGenericError{ entry, "failed to read value" };
            }
        }

        template<typename T>
        T parseMandatoryValue(const Wt::Json::Object& object, const std::string& entry)
        {
            std::optional<T> res{ parseValue<T>(object, entry) };
            if (!res)
                throw BadParameterGenericError{ entry, "field is mandatory" };

            return *res;
        }

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

        std::vector<std::string> parseValues(std::string_view str, char delimiter)
        {
            std::vector<std::string> res;
            for (std::string_view v : core::stringUtils::splitString(str, delimiter))
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

            res.name = parseMandatoryValue<std::string>(root, "name");
            res.platform = parseMandatoryValue<std::string>(root, "platform");
            {
                const auto maxAudioBitrate = parseValue<long>(root, "maxAudioBitrate");
                if (maxAudioBitrate && *maxAudioBitrate > 0)
                    res.maxAudioBitrate = *maxAudioBitrate;
            }

            {
                const auto maxTranscodingAudioBitrate = parseValue<long>(root, "maxTranscodingAudioBitrate");
                if (maxTranscodingAudioBitrate && *maxTranscodingAudioBitrate > 0)
                    res.maxTranscodingAudioBitrate = *maxTranscodingAudioBitrate;
            }

            if (const Wt::Json::Value & directPlayProfiles{ root.get("directPlayProfiles") }; directPlayProfiles.type() != Wt::Json::Type::Null)
            {
                for (const Wt::Json::Object& profile : static_cast<const Wt::Json::Array&>(directPlayProfiles))
                {
                    DirectPlayProfile directPlayProfile;

                    // containers
                    {
                        const auto container{ parseMandatoryValue<std::string>(profile, "container") };
                        for (std::string_view container : core::stringUtils::splitString(container, ','))
                            directPlayProfile.containers.push_back(std::string{ container });

                        if (directPlayProfile.containers.empty())
                            throw BadParameterGenericError{ "ClientInfo::DirectPlayProfile::container", "cannot be empty" };
                        if (directPlayProfile.containers.size() > 1 && std::any_of(std::cbegin(directPlayProfile.containers), std::cend(directPlayProfile.containers), [](const std::string& container) { return container == "*"; }))
                            throw BadParameterGenericError{ "ClientInfo::DirectPlayProfile::container", "cannot have * when multiple containers are specified" };
                    }

                    // audio codecs
                    {
                        const auto audioCodec{ parseMandatoryValue<std::string>(profile, "audioCodec") };
                        for (std::string_view codec : core::stringUtils::splitString(audioCodec, ','))
                            directPlayProfile.audioCodecs.push_back(std::string{ codec });

                        if (directPlayProfile.audioCodecs.empty())
                            throw BadParameterGenericError{ "ClientInfo::DirectPlayProfile::audioCodec", "cannot be empty" };
                        if (directPlayProfile.audioCodecs.size() > 1 && std::any_of(std::cbegin(directPlayProfile.audioCodecs), std::cend(directPlayProfile.audioCodecs), [](const std::string& codec) { return codec == "*"; }))
                            throw BadParameterGenericError{ "ClientInfo::DirectPlayProfile::audioCodec", "cannot have * when multiple codecs are specified" };
                    }

                    directPlayProfile.protocol = parseMandatoryValue<std::string>(profile, "protocol");
                    if (directPlayProfile.protocol.empty())
                        throw BadParameterGenericError{ "ClientInfo::DirectPlayProfile::protocol", "cannot be empty" };

                    {
                        const auto maxAudioChannels = parseValue<long>(profile, "maxAudioChannels");
                        if (maxAudioChannels && *maxAudioChannels > 0)
                            directPlayProfile.maxAudioChannels = *maxAudioChannels;
                    }

                    res.directPlayProfiles.push_back(directPlayProfile);
                }
            }

            if (const Wt::Json::Value & transcodingProfiles{ root.get("transcodingProfiles") }; transcodingProfiles.type() != Wt::Json::Type::Null)
            {
                for (const Wt::Json::Object& profile : static_cast<const Wt::Json::Array&>(transcodingProfiles))
                {
                    TranscodingProfile transcodingProfile;
                    transcodingProfile.container = parseMandatoryValue<std::string>(profile, "container");
                    if (transcodingProfile.container.empty())
                        throw BadParameterGenericError{ "ClientInfo::TranscodingProfile::container", "cannot be empty" };

                    if (transcodingProfile.container.find('*') != std::string::npos)
                        throw BadParameterGenericError{ "ClientInfo::TranscodingProfile::container", "cannot have *" };
                    if (transcodingProfile.container.find(',') != std::string::npos)
                        throw BadParameterGenericError{ "ClientInfo::TranscodingProfile::container", "cannot have ," };

                    transcodingProfile.audioCodec = parseMandatoryValue<std::string>(profile, "audioCodec");
                    if (transcodingProfile.audioCodec.empty())
                        throw BadParameterGenericError{ "ClientInfo::TranscodingProfile::audioCodec", "cannot be empty" };
                    if (transcodingProfile.audioCodec.find('*') != std::string::npos)
                        throw BadParameterGenericError{ "ClientInfo::TranscodingProfile::audioCodec", "cannot have *" };
                    if (transcodingProfile.audioCodec.find(',') != std::string::npos)
                        throw BadParameterGenericError{ "ClientInfo::TranscodingProfile::audioCodec", "cannot have ," };

                    transcodingProfile.protocol = parseMandatoryValue<std::string>(profile, "protocol");
                    if (transcodingProfile.protocol.empty())
                        throw BadParameterGenericError{ "ClientInfo::TranscodingProfile::protocol", "cannot be empty" };

                    {
                        const auto maxAudioChannels{ parseValue<long>(profile, "maxAudioChannels") };
                        if (maxAudioChannels && *maxAudioChannels > 0)
                            transcodingProfile.maxAudioChannels = *maxAudioChannels;
                    }

                    res.transcodingProfiles.push_back(transcodingProfile);
                }
            }

            // codecProfiles
            if (const Wt::Json::Value & codecProfiles{ root.get("codecProfiles") }; codecProfiles.type() != Wt::Json::Type::Null)
            {
                for (const Wt::Json::Object& profile : static_cast<const Wt::Json::Array&>(codecProfiles))
                {
                    CodecProfile codecProfile;
                    codecProfile.type = parseMandatoryValue<std::string>(profile, "type");
                    if (codecProfile.type != "AudioCodec" && codecProfile.type != "VideoCodec")
                        throw BadParameterGenericError{ "ClientInfo::CodecProfile::type", "unexpected value" };

                    codecProfile.name = parseMandatoryValue<std::string>(profile, "name");
                    if (codecProfile.name.empty())
                        throw BadParameterGenericError{ "ClientInfo::CodecProfile::name", "name must not be empty" };

                    if (const Wt::Json::Value & limitations{ profile.get("limitations") }; limitations.type() != Wt::Json::Type::Null)
                    {
                        for (const Wt::Json::Object& limitation : static_cast<const Wt::Json::Array&>(limitations))
                        {
                            Limitation lim;
                            lim.name = parseLimitationType(parseMandatoryValue<std::string>(limitation, "name"));
                            lim.comparison = parseComparisonOperator(parseMandatoryValue<std::string>(limitation, "comparison"));
                            lim.values = parseValues(parseMandatoryValue<std::string>(limitation, "value"), '|');
                            lim.required = parseMandatoryValue<bool>(limitation, "required");

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