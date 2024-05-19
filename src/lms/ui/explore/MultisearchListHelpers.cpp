#include "MultisearchListHelpers.hpp"

#include <LmsApplication.hpp>
#include <MediaPlayer.hpp>
#include <common/Template.hpp>
#include <core/Service.hpp>
#include <database/Release.hpp>
#include <database/Session.hpp>
#include <database/Track.hpp>
#include <resource/CoverResource.hpp>
#include <resource/DownloadResource.hpp>
#include <services/feedback/IFeedbackService.hpp>
#include <Wt/WAnchor.h>
#include <Wt/WPushButton.h>

#include "PlayQueueController.hpp"
#include "ReleaseHelpers.hpp"
#include "TrackListHelpers.hpp"
#include "Utils.hpp"


namespace lms::ui::MultisearchListHelpers {

    std::unique_ptr<Wt::WWidget> createEntry(const db::ObjectPtr<db::Track>& track, PlayQueueController& playQueueController, Filters& filters)
    {
        auto entry = std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Multisearch.template.entry-track"));

        entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

        const db::Release::pointer release{ track->getRelease() };
        const db::TrackId trackId{ track->getId() };

        const auto artists{ track->getArtistIds({db::TrackArtistLinkType::Artist}) };
        if (!artists.empty())
        {
            entry->setCondition("if-has-artists", true);
            entry->bindWidget("artists", utils::createArtistDisplayNameWithAnchors(track->getArtistDisplayName(), artists));
            entry->bindWidget("artists-md", utils::createArtistDisplayNameWithAnchors(track->getArtistDisplayName(), artists));
        }

        if (track->getRelease())
        {
            entry->setCondition("if-has-release", true);
            entry->bindWidget("release", utils::createReleaseAnchor(track->getRelease()));
            Wt::WAnchor* anchor{ entry->bindWidget("cover", utils::createReleaseAnchor(release, false)) };
            auto cover{ utils::createCover(release->getId(), CoverResource::Size::Small) };
            cover->addStyleClass("Lms-cover-track Lms-cover-anchor"); // HACK
            anchor->setImage(std::move((cover)));
        }
        else
        {
            auto cover{ utils::createCover(trackId, CoverResource::Size::Small) };
            cover->addStyleClass("Lms-cover-track"); // HACK
            entry->bindWidget<Wt::WImage>("cover", std::move(cover));
        }

        entry->bindString("duration", utils::durationToString(track->getDuration()), Wt::TextFormat::Plain);

        Wt::WPushButton* playBtn{ entry->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.template.play-btn"), Wt::TextFormat::XHTML) };
        playBtn->clicked().connect([trackId, &playQueueController]
            {
                playQueueController.processCommand(PlayQueueController::Command::Play, { trackId });
            });

        entry->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.template.more-btn"), Wt::TextFormat::XHTML);
        entry->bindNew<Wt::WPushButton>("play", Wt::WString::tr("Lms.Explore.play"))
            ->clicked().connect([trackId, &playQueueController]
                {
                    playQueueController.processCommand(PlayQueueController::Command::Play, { trackId });
                });
        entry->bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"))
            ->clicked().connect([=, &playQueueController]
                {
                    playQueueController.processCommand(PlayQueueController::Command::PlayNext, { trackId });
                });
        entry->bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"))
            ->clicked().connect([=, &playQueueController]
                {
                    playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, { trackId });
                });

        {
            auto isStarred{ [=] { return core::Service<feedback::IFeedbackService>::get()->isStarred(LmsApp->getUserId(), trackId); } };

            Wt::WPushButton* starBtn{ entry->bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star")) };
            starBtn->clicked().connect([=]
                {
                    auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                    if (isStarred())
                    {
                        core::Service<feedback::IFeedbackService>::get()->unstar(LmsApp->getUserId(), trackId);
                        starBtn->setText(Wt::WString::tr("Lms.Explore.star"));
                    }
                    else
                    {
                        core::Service<feedback::IFeedbackService>::get()->star(LmsApp->getUserId(), trackId);
                        starBtn->setText(Wt::WString::tr("Lms.Explore.unstar"));
                    }
                });
        }

        entry->bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
            ->setLink(Wt::WLink{ std::make_unique<DownloadTrackResource>(trackId) });

        entry->bindNew<Wt::WPushButton>("track-info", Wt::WString::tr("Lms.Explore.track-info"))
            ->clicked().connect([trackId, &filters] { TrackListHelpers::showTrackInfoModal(trackId, filters); });

        return entry;
    }

    std::unique_ptr<Wt::WWidget> createEntry(const db::ObjectPtr<db::Release>& release, PlayQueueController& playQueueController, Filters&) {
        auto entry = std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Multisearch.template.entry-release"));
        entry->addFunction("tr", &Wt::WTemplate::Functions::tr);
        entry->addFunction("id", &Wt::WTemplate::Functions::id);

        entry->bindWidget("name", utils::createReleaseAnchor(release));
        Wt::WAnchor* anchor{ entry->bindWidget("cover", utils::createReleaseAnchor(release, false)) };
        auto cover{ utils::createCover(release->getId(), CoverResource::Size::Small) };
        cover->addStyleClass("Lms-cover-track Lms-cover-anchor"); // HACK
        anchor->setImage(std::move(cover));

        const auto artists = release->getArtists({db::TrackArtistLinkType::Artist});;
        if (!artists.empty())
        {
            entry->setCondition("if-has-artists", true);
            entry->bindWidget("artists", utils::createArtistDisplayNameWithAnchors(release->getArtistDisplayName(), artists));
            entry->bindWidget("artists-md", utils::createArtistDisplayNameWithAnchors(release->getArtistDisplayName(), artists));
        }

        entry->bindString("duration", utils::durationToString(release->getDuration()), Wt::TextFormat::Plain);

        auto releaseId = release->getId();

        Wt::WPushButton* playBtn{ entry->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.template.play-btn"), Wt::TextFormat::XHTML) };
        playBtn->clicked().connect([releaseId, &playQueueController]
            {
                playQueueController.processCommand(PlayQueueController::Command::Play, { releaseId });
            });

        entry->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.template.more-btn"), Wt::TextFormat::XHTML);
        entry->bindNew<Wt::WPushButton>("play", Wt::WString::tr("Lms.Explore.play"))
            ->clicked().connect([releaseId, &playQueueController]
                {
                    playQueueController.processCommand(PlayQueueController::Command::Play, { releaseId });
                });
        entry->bindNew<Wt::WPushButton>("play-next", Wt::WString::tr("Lms.Explore.play-next"))
            ->clicked().connect([releaseId, &playQueueController]
                {
                    playQueueController.processCommand(PlayQueueController::Command::PlayNext, { releaseId });
                });
        entry->bindNew<Wt::WPushButton>("play-last", Wt::WString::tr("Lms.Explore.play-last"))
            ->clicked().connect([releaseId, &playQueueController]
                {
                    playQueueController.processCommand(PlayQueueController::Command::PlayOrAddLast, { releaseId });
                });
        entry->bindNew<Wt::WPushButton>("play-shuffled", Wt::WString::tr("Lms.Explore.play-shuffled"))
            ->clicked().connect([releaseId, &playQueueController]
                {
                    playQueueController.processCommand(PlayQueueController::Command::PlayShuffled, { releaseId });
                });

        {
            auto isStarred{ [=] { return core::Service<feedback::IFeedbackService>::get()->isStarred(LmsApp->getUserId(), releaseId); } };

            Wt::WPushButton* starBtn{ entry->bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star")) };
            starBtn->clicked().connect([=]
                {
                    auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                    if (isStarred())
                    {
                        core::Service<feedback::IFeedbackService>::get()->unstar(LmsApp->getUserId(), releaseId);
                        starBtn->setText(Wt::WString::tr("Lms.Explore.star"));
                    }
                    else
                    {
                        core::Service<feedback::IFeedbackService>::get()->star(LmsApp->getUserId(), releaseId);
                        starBtn->setText(Wt::WString::tr("Lms.Explore.unstar"));
                    }
                });
        }

        if (const auto mbid = release->getMBID())
        {
            entry->setCondition("if-has-mbid", true);
            entry->bindString("mbid-link", std::string{ "https://musicbrainz.org/release/" } + std::string{ mbid->getAsString() });
        }

        entry->bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
            ->setLink(Wt::WLink{ std::make_unique<DownloadReleaseResource>(releaseId) });

        entry->bindNew<Wt::WPushButton>("release-info", Wt::WString::tr("Lms.Explore.release-info"))
            ->clicked().connect([releaseId]
                {
                    releaseHelpers::showReleaseInfoModal(releaseId);
                });

        return entry;
    }

    std::unique_ptr<Wt::WWidget> createEntry(const db::ObjectPtr<db::Artist>& artist, PlayQueueController&, Filters&) {
        auto entry = std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Multisearch.template.entry-artist"));

        entry->bindWidget("name", utils::createArtistAnchor(artist));
        Wt::WAnchor* anchor{ entry->bindWidget("cover", utils::createArtistAnchor(artist, false)) };
        auto cover{ utils::createCover(artist->getId(), CoverResource::Size::Small) };
        cover->addStyleClass("Lms-cover-track Lms-cover-anchor"); // HACK
        anchor->setImage(std::move(cover));

        return entry;
    }
}
