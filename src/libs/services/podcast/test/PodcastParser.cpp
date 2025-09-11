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

#include <chrono>

#include <Wt/WDate.h>
#include <Wt/WTime.h>
#include <gtest/gtest.h>

#include "PodcastParsing.hpp"

namespace lms::podcast::tests
{
    TEST(Podcast, PodcastParsing)
    {
        constexpr std::string_view xmlData{ R"(<rss xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd" xmlns:pa="http://podcastaddict.com" xmlns:podcastRF="http://radiofrance.fr/Lancelot/Podcast#" xmlns:googleplay="http://www.google.com/schemas/play-podcasts/1.0" version="2.0">
<channel>
<title>Affaires sensibles</title>
<link>https://www.franceinter.fr/emission-affaires-sensibles</link>
<description>Les grandes affaires, les aventures et les procès qui ont marqué les cinquante dernières années. Vous aimez ce podcast ? Pour écouter tous les épisodes sans limite, rendez-vous sur <a href="https://www.franceinter.fr/emission-affaires-sensibles?at_campaign=desc_podcast&at_medium=lien_RSS">Radio France</a>.</description>
<language>fr</language>
<copyright>Radio France</copyright>
<lastBuildDate>Sat, 09 Aug 2025 21:34:32 +0200</lastBuildDate>
<generator>Radio France</generator>
<image>
<url>https://www.radiofrance.fr/s3/cruiser-production/2023/05/5f197ac5-b950-4149-8578-ae12aa6695b0/1400x1400_sc_rf_omm_0000038645_ite.jpg</url>
<title>Affaires sensibles</title>
<link>https://www.franceinter.fr/emission-affaires-sensibles</link>
</image>
<itunes:author>France Inter</itunes:author>
<itunes:category text="Society & Culture"/>
<itunes:explicit>no</itunes:explicit>
<itunes:image href="https://www.radiofrance.fr/s3/cruiser-production/2023/05/5f197ac5-b950-4149-8578-ae12aa6695b0/1400x1400_sc_rf_omm_0000038645_ite.jpg"/>
<itunes:owner>
<itunes:email>podcast@radiofrance.com</itunes:email>
<itunes:name>Radio France</itunes:name>
</itunes:owner>
<itunes:subtitle>Affaires sensibles</itunes:subtitle>
<itunes:summary>Les grandes affaires, les aventures et les procès qui ont marqué les cinquante dernières années. Vous aimez ce podcast ? Pour écouter tous les épisodes sans limite, rendez-vous sur <a href="https://www.franceinter.fr/emission-affaires-sensibles?at_campaign=desc_podcast&at_medium=lien_RSS">Radio France</a>.</itunes:summary>
<itunes:new-feed-url>https://radiofrance-podcast.net/podcast09/35099478-7c72-4f9e-a6de-1b928400e9e5/rss_13940.xml</itunes:new-feed-url>
<pa:new-feed-url>https://radiofrance-podcast.net/podcast09/d4463877-caa3-4507-9399-f5eb00fde027/rss_13940.xml</pa:new-feed-url>
<podcastRF:originStation>1</podcastRF:originStation>
<googleplay:block>yes</googleplay:block>
<item>
<title>Apollo 13 ou les naufragés de l’espace</title>
<link>https://www.radiofrance.fr/franceinter/podcasts/affaires-sensibles/affaires-sensibles-du-jeudi-30-decembre-2021-4186094</link>
<description>durée : 00:53:58 - Affaires sensibles - par : Christophe Barreyre, Fabrice Drouelle - C'est une épopée qui réunit héroïsme, génie technologique, audace diplomatique, sens du tragique. L’odyssée d'Apollo 13 c’est un vaisseau spatial en perdition, là-haut, flirtant avec la lune mais qui au dernier moment se dérobe pour une vulgaire panne électrique dans la soute du vaisseau. - réalisé par : Flora BERNARD, Marion Le Lay, Stéphane COSME Vous aimez ce podcast ? Pour écouter tous les autres épisodes sans limite, rendez-vous sur <a href="https://www.franceinter.fr/emission-affaires-sensibles?at_campaign=desc_episode&at_medium=lien_RSS">Radio France</a>.</description>
<author>podcast@radiofrance.com (Radio France)</author>
<category>Society & Culture</category>
<enclosure url="https://proxycast.radiofrance.fr/e2a713a6-aba0-4d2a-a4a1-d135d98f1f8a/13940-09.08.2025-ITEMA_24214125-2021F22805S0364-NET_MFI_F7191B05-DD5B-4AFE-BB96-3BD8ADB3240A-22.mp3" length="51842568" type="audio/mpeg"/>
<guid isPermaLink="false">9f587824-01f5-443d-aa72-a56519d25857-NET_MFI_F7191B05-DD5B-4AFE-BB96-3BD8ADB3240A</guid>
<pubDate>Sat, 09 Aug 2025 15:59:59 +0200</pubDate>
<podcastRF:businessReference>22805</podcastRF:businessReference>
<podcastRF:magnetothequeID>2021F22805S0364</podcastRF:magnetothequeID>
<itunes:title>Apollo 13 ou les naufragés de l’espace</itunes:title>
<itunes:image href="https://www.radiofrance.fr/s3/cruiser-production/2021/04/ec0f1c5d-ecfa-4ec4-a5a5-30f446d25aea/1400x1400_affaires_sensibles.jpg"/>
<itunes:author>Christophe Barreyre, Fabrice Drouelle</itunes:author>
<itunes:explicit>no</itunes:explicit>
<itunes:keywords>Apollo,13,ou,les,naufragés,de,l’espace</itunes:keywords>
<itunes:subtitle>Apollo 13 ou les naufragés de l’espace</itunes:subtitle>
<itunes:summary>durée : 00:53:58 - Affaires sensibles - par : Christophe Barreyre, Fabrice Drouelle - C'est une épopée qui réunit héroïsme, génie technologique, audace diplomatique, sens du tragique. L’odyssée d'Apollo 13 c’est un vaisseau spatial en perdition, là-haut, flirtant avec la lune mais qui au dernier moment se dérobe pour une vulgaire panne électrique dans la soute du vaisseau. - réalisé par : Flora BERNARD, Marion Le Lay, Stéphane COSME Vous aimez ce podcast ? Pour écouter tous les autres épisodes sans limite, rendez-vous sur <a href="https://www.franceinter.fr/emission-affaires-sensibles?at_campaign=desc_episode&at_medium=lien_RSS">Radio France</a>.</itunes:summary>
<itunes:duration>00:53:58</itunes:duration>
<googleplay:block>yes</googleplay:block>
</item>
<item>
<title>Stéphane Breitwieser, le pilleur de musées</title>
<link>https://www.radiofrance.fr/franceinter/podcasts/affaires-sensibles/affaires-sensibles-du-mercredi-02-avril-2025-9048848</link>
<description>durée : 00:47:45 - Affaires sensibles - par : Fabrice Drouelle, Franck COGNARD - Aujourd’hui dans Affaires Sensibles : l’affaire Stéphane Breitwieser, le pilleur de musées - réalisé par : Etienne BERTIN Vous aimez ce podcast ? Pour écouter tous les autres épisodes sans limite, rendez-vous sur <a href="https://www.franceinter.fr/emission-affaires-sensibles?at_campaign=desc_episode&at_medium=lien_RSS">Radio France</a>.</description>
<author>podcast@radiofrance.com (Radio France)</author>
<category>Society & Culture</category>
<enclosure url="https://proxycast.radiofrance.fr/d0895b0b-a99c-4e9d-9d99-13a029960d04/13940-08.08.2025-ITEMA_24213067-2025F22805S0092-NET_MFI_FF964E00-DFB6-45AC-A9D2-2D1211F37586-22.mp3" length="45869054" type="audio/mpeg"/>
<guid isPermaLink="false">719c21be-3832-4b67-8d65-9d07b3715d7b-NET_MFI_FF964E00-DFB6-45AC-A9D2-2D1211F37586</guid>
<pubDate>Fri, 08 Aug 2025 20:59:59 +0200</pubDate>
<podcastRF:businessReference>22805</podcastRF:businessReference>
<podcastRF:magnetothequeID>2025F22805S0092</podcastRF:magnetothequeID>
<itunes:title>Stéphane Breitwieser, le pilleur de musées</itunes:title>
<itunes:image href="https://www.radiofrance.fr/s3/cruiser-production/2023/04/7b50cf5f-f5bd-4dc4-8b1d-b08666768dcf/1400x1400_sc_affaires-sensibles.jpg"/>
<itunes:author>Fabrice Drouelle, Franck COGNARD</itunes:author>
<itunes:explicit>no</itunes:explicit>
<itunes:keywords>Stéphane,Breitwieser,,le,pilleur,de,musées</itunes:keywords>
<itunes:subtitle>Stéphane Breitwieser, le pilleur de musées</itunes:subtitle>
<itunes:summary>durée : 00:47:45 - Affaires sensibles - par : Fabrice Drouelle, Franck COGNARD - Aujourd’hui dans Affaires Sensibles : l’affaire Stéphane Breitwieser, le pilleur de musées - réalisé par : Etienne BERTIN Vous aimez ce podcast ? Pour écouter tous les autres épisodes sans limite, rendez-vous sur <a href="https://www.franceinter.fr/emission-affaires-sensibles?at_campaign=desc_episode&at_medium=lien_RSS">Radio France</a>.</itunes:summary>
<itunes:duration>00:47:45</itunes:duration>
<googleplay:block>yes</googleplay:block>
</item>
</channel>
</rss>)" };

        const Podcast podcast{ parsePodcastRssFeed(xmlData) };
        EXPECT_EQ(podcast.title, "Affaires sensibles");
        EXPECT_EQ(podcast.description, R"(Les grandes affaires, les aventures et les procès qui ont marqué les cinquante dernières années. Vous aimez ce podcast ? Pour écouter tous les épisodes sans limite, rendez-vous sur <a href="https://www.franceinter.fr/emission-affaires-sensibles?at_campaign=desc_podcast&amp;at_medium=lien_RSS">Radio France</a>.)");
        EXPECT_EQ(podcast.author, "France Inter");
        EXPECT_EQ(podcast.link, "https://www.franceinter.fr/emission-affaires-sensibles");
        EXPECT_EQ(podcast.language, "fr");
        EXPECT_EQ(podcast.copyright, "Radio France");
        EXPECT_EQ(podcast.lastBuildDate, (Wt::WDateTime{ Wt::WDate{ 2025, 8, 9 }, Wt::WTime{ 23, 34, 32 } }));
        EXPECT_EQ(podcast.imageUrl, "https://www.radiofrance.fr/s3/cruiser-production/2023/05/5f197ac5-b950-4149-8578-ae12aa6695b0/1400x1400_sc_rf_omm_0000038645_ite.jpg");
        // itunes
        EXPECT_EQ(podcast.copyright, "Radio France");
        EXPECT_EQ(podcast.author, "France Inter");
        EXPECT_EQ(podcast.category, "Society & Culture");
        EXPECT_EQ(podcast.explicitContent, false);
        EXPECT_EQ(podcast.ownerEmail, "podcast@radiofrance.com");
        EXPECT_EQ(podcast.ownerName, "Radio France");
        EXPECT_EQ(podcast.subtitle, "Affaires sensibles");
        EXPECT_EQ(podcast.summary, R"(Les grandes affaires, les aventures et les procès qui ont marqué les cinquante dernières années. Vous aimez ce podcast ? Pour écouter tous les épisodes sans limite, rendez-vous sur <a href="https://www.franceinter.fr/emission-affaires-sensibles?at_campaign=desc_podcast&amp;at_medium=lien_RSS">Radio France</a>.)");

        ASSERT_EQ(podcast.episodes.size(), 2);
        EXPECT_EQ(podcast.episodes[0].title, R"(Apollo 13 ou les naufragés de l’espace)");
        EXPECT_EQ(podcast.episodes[0].description, R"(durée : 00:53:58 - Affaires sensibles - par : Christophe Barreyre, Fabrice Drouelle - C'est une épopée qui réunit héroïsme, génie technologique, audace diplomatique, sens du tragique. L’odyssée d'Apollo 13 c’est un vaisseau spatial en perdition, là-haut, flirtant avec la lune mais qui au dernier moment se dérobe pour une vulgaire panne électrique dans la soute du vaisseau. - réalisé par : Flora BERNARD, Marion Le Lay, Stéphane COSME Vous aimez ce podcast ? Pour écouter tous les autres épisodes sans limite, rendez-vous sur <a href="https://www.franceinter.fr/emission-affaires-sensibles?at_campaign=desc_episode&amp;at_medium=lien_RSS">Radio France</a>.)");
        EXPECT_EQ(podcast.episodes[0].author, "Christophe Barreyre, Fabrice Drouelle");
        EXPECT_EQ(podcast.episodes[0].explicitContent, false);
        EXPECT_EQ(podcast.episodes[0].imageUrl, "https://www.radiofrance.fr/s3/cruiser-production/2021/04/ec0f1c5d-ecfa-4ec4-a5a5-30f446d25aea/1400x1400_affaires_sensibles.jpg");
        EXPECT_EQ(podcast.episodes[0].link, "https://www.radiofrance.fr/franceinter/podcasts/affaires-sensibles/affaires-sensibles-du-jeudi-30-decembre-2021-4186094");
        EXPECT_EQ(podcast.episodes[0].pubDate, (Wt::WDateTime{ Wt::WDate{ 2025, 8, 9 }, Wt::WTime{ 17, 59, 59 } }));
        EXPECT_EQ(podcast.episodes[0].guid, "9f587824-01f5-443d-aa72-a56519d25857-NET_MFI_F7191B05-DD5B-4AFE-BB96-3BD8ADB3240A");
        EXPECT_EQ(podcast.episodes[0].enclosureUrl.url, "https://proxycast.radiofrance.fr/e2a713a6-aba0-4d2a-a4a1-d135d98f1f8a/13940-09.08.2025-ITEMA_24214125-2021F22805S0364-NET_MFI_F7191B05-DD5B-4AFE-BB96-3BD8ADB3240A-22.mp3");
        EXPECT_EQ(podcast.episodes[0].enclosureUrl.length, 51842568);
        EXPECT_EQ(podcast.episodes[0].enclosureUrl.type, "audio/mpeg");
        EXPECT_EQ(podcast.episodes[0].duration, std::chrono::minutes{ 53 } + std::chrono::seconds{ 58 });

        EXPECT_EQ(podcast.episodes[1].title, R"(Stéphane Breitwieser, le pilleur de musées)");
        EXPECT_EQ(podcast.episodes[1].description, R"(durée : 00:47:45 - Affaires sensibles - par : Fabrice Drouelle, Franck COGNARD - Aujourd’hui dans Affaires Sensibles : l’affaire Stéphane Breitwieser, le pilleur de musées - réalisé par : Etienne BERTIN Vous aimez ce podcast ? Pour écouter tous les autres épisodes sans limite, rendez-vous sur <a href="https://www.franceinter.fr/emission-affaires-sensibles?at_campaign=desc_episode&amp;at_medium=lien_RSS">Radio France</a>.)");
        EXPECT_EQ(podcast.episodes[1].author, "Fabrice Drouelle, Franck COGNARD");
        EXPECT_EQ(podcast.episodes[1].explicitContent, false);
        EXPECT_EQ(podcast.episodes[1].imageUrl, "https://www.radiofrance.fr/s3/cruiser-production/2023/04/7b50cf5f-f5bd-4dc4-8b1d-b08666768dcf/1400x1400_sc_affaires-sensibles.jpg");
        EXPECT_EQ(podcast.episodes[1].link, "https://www.radiofrance.fr/franceinter/podcasts/affaires-sensibles/affaires-sensibles-du-mercredi-02-avril-2025-9048848");
        EXPECT_EQ(podcast.episodes[1].pubDate, (Wt::WDateTime{ Wt::WDate{ 2025, 8, 8 }, Wt::WTime{ 22, 59, 59 } }));
        EXPECT_EQ(podcast.episodes[1].guid, "719c21be-3832-4b67-8d65-9d07b3715d7b-NET_MFI_FF964E00-DFB6-45AC-A9D2-2D1211F37586");
        EXPECT_EQ(podcast.episodes[1].enclosureUrl.url, "https://proxycast.radiofrance.fr/d0895b0b-a99c-4e9d-9d99-13a029960d04/13940-08.08.2025-ITEMA_24213067-2025F22805S0092-NET_MFI_FF964E00-DFB6-45AC-A9D2-2D1211F37586-22.mp3");
        EXPECT_EQ(podcast.episodes[1].enclosureUrl.length, 45869054);
        EXPECT_EQ(podcast.episodes[1].enclosureUrl.type, "audio/mpeg");
        EXPECT_EQ(podcast.episodes[1].duration, std::chrono::minutes{ 47 } + std::chrono::seconds{ 45 });
    }
} // namespace lms::podcast::tests