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

#include "PodcastParsing.hpp"

#include <charconv>
#include <optional>

#include <pugixml.hpp>

#include "core/ILogger.hpp"
#include "core/String.hpp"

namespace lms::podcast
{
    namespace
    {
        std::optional<std::chrono::seconds> parseDuration(std::string_view str)
        {
            auto parse_int{ [](std::string_view sv) -> std::optional<int> {
                int value{};
                const auto [ptr, ec]{ std::from_chars(sv.data(), sv.data() + sv.size(), value) };
                return (ec == std::errc()) ? std::optional{ value } : std::nullopt;
            } };

            std::array<int, 3> parts{ 0, 0, 0 };
            int index{ 3 };
            while (!str.empty() && --index >= 0)
            {
                const std::size_t pos{ str.rfind(':') };
                const std::string_view token{ (pos == std::string_view::npos) ? str : str.substr(pos + 1) };

                const auto val{ parse_int(token) };
                if (!val)
                    return std::nullopt;

                parts[index] = *val;
                if (pos == std::string_view::npos)
                    break;

                str.remove_suffix(str.size() - pos);
            }

            return std::chrono::hours{ parts[0] } + std::chrono::minutes{ parts[1] } + std::chrono::seconds{ parts[2] };
        }

        std::optional<std::chrono::seconds> getDuration(const pugi::xml_node& node, const char* tag)
        {
            std::optional<std::chrono::seconds> res;

            if (const pugi::xml_node child{ node.child(tag) })
            {
                std::string_view value{ child.child_value() };
                res = parseDuration(value);
            }

            return res;
        }

        std::optional<bool> getBool(const pugi::xml_node& node, const char* tag)
        {
            std::optional<bool> res;

            if (const pugi::xml_node child{ node.child(tag) })
            {
                std::string_view value{ child.child_value() };
                if (value == "true" || value == "1" || value == "on" || value == "yes")
                    res = true;
                else if (value == "false" || value == "0" || value == "off" || value == "no")
                    res = false;
            }

            return res;
        }

        std::string_view getText(const pugi::xml_node& node, const char* tag)
        {
            std::string_view res;

            if (const pugi::xml_node child{ node.child(tag) })
                res = child.child_value();

            return res;
        }

        std::string getRawText(const pugi::xml_node& node, const char* tag)
        {
            std::string res;

            if (const pugi::xml_node child{ node.child(tag) })
            {
                std::ostringstream oss;
                for (const pugi::xml_node& n : child.children())
                    n.print(oss, "", pugi::format_raw);
                res = oss.str();
            }

            return res;
        }

        std::string_view getAttribute(const pugi::xml_node& node, const char* tag, const char* attribute)
        {
            std::string_view res;

            if (const pugi::xml_node child{ node.child(tag) })
                res = child.attribute(attribute).value();

            return res;
        }
    } // namespace

    Podcast parsePodcastRssFeed(std::string_view rssXml)
    {
        Podcast podcast;

        pugi::xml_document doc;
        pugi::xml_parse_result result{ doc.load_buffer(rssXml.data(), rssXml.size()) };
        if (!result)
        {
            LMS_LOG(METADATA, ERROR, "Cannot read xml: " << result.description());
            throw ParseException{ result.description() };
        }

        const pugi::xml_node channel{ doc.child("rss").child("channel") };
        if (!channel)
            throw ParseException{ "No <channel> element found in podcast XML" };

        podcast.title = getText(channel, "title");
        podcast.link = getText(channel, "link");
        podcast.description = getRawText(channel, "description");
        podcast.language = getText(channel, "language");
        podcast.copyright = getText(channel, "copyright");
        podcast.lastBuildDate = core::stringUtils::fromRFC822String(getText(channel, "lastBuildDate"));

        // itunes fields
        podcast.newUrl = getText(channel, "itunes:new-feed-url");
        podcast.author = getText(channel, "itunes:author");
        podcast.category = getAttribute(channel, "itunes:category", "text");
        podcast.imageUrl = getText(channel, "itunes:image");
        if (podcast.imageUrl.empty())
        {
            if (const pugi::xml_node image{ channel.child("image") })
                podcast.imageUrl = getText(image, "url");
        }
        if (const pugi::xml_node owner{ channel.child("itunes:owner") })
        {
            podcast.ownerEmail = getText(owner, "itunes:email");
            podcast.ownerName = getText(owner, "itunes:name");
        }
        podcast.subtitle = getText(channel, "itunes:subtitle");
        podcast.summary = getRawText(channel, "itunes:summary");
        podcast.explicitContent = getBool(channel, "itunes:explicit");

        // parse nested episodes
        for (pugi::xml_node episode{ channel.child("item") }; episode; episode = episode.next_sibling("item"))
        {
            PodcastEpisode e;
            e.title = getText(episode, "title");
            // <enclosure url="https://proxycast.radiofrance.fr/e2a713a6-aba0-4d2a-a4a1-d135d98f1f8a/13940-09.08.2025-ITEMA_24214125-2021F22805S0364-NET_MFI_F7191B05-DD5B-4AFE-BB96-3BD8ADB3240A-22.mp3" length="51842568" type="audio/mpeg"/>
            if (const pugi::xml_node enclosure{ episode.child("enclosure") })
                e.url = getText(enclosure, "url");
            e.pubDate = core::stringUtils::fromRFC822String(getText(episode, "pubDate"));
            e.description = getRawText(episode, "description");
            e.link = getText(episode, "link");
            e.author = getText(episode, "itunes:author");
            if (e.author.empty())
                e.author = getText(episode, "author");

            e.enclosureUrl.url = getAttribute(episode, "enclosure", "url");
            e.enclosureUrl.length = core::stringUtils::readAs<std::size_t>(getAttribute(episode, "enclosure", "length")).value_or(0);
            e.enclosureUrl.type = getAttribute(episode, "enclosure", "type");

            e.category = getAttribute(episode, "itunes:category", "text");
            e.duration = getDuration(episode, "itunes:duration").value_or(std::chrono::seconds::zero());
            e.guid = getText(episode, "guid");

            e.imageUrl = getAttribute(episode, "itunes:image", "href");
            e.explicitContent = getBool(episode, "itunes:explicit");

            podcast.episodes.push_back(std::move(e));
        }

        return podcast;
    }

} // namespace lms::podcast