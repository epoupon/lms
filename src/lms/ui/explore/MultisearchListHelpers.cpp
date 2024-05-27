#include "MultisearchListHelpers.hpp"

#include "ArtistListHelpers.hpp"

#include <LmsApplication.hpp>
#include <MediaPlayer.hpp>
#include <Wt/WAnchor.h>
#include <Wt/WPushButton.h>
#include <common/Template.hpp>
#include <core/Service.hpp>
#include <database/Release.hpp>
#include <database/Session.hpp>
#include <database/Track.hpp>
#include <resource/CoverResource.hpp>
#include <resource/DownloadResource.hpp>
#include <services/feedback/IFeedbackService.hpp>

#include "PlayQueueController.hpp"
#include "ReleaseHelpers.hpp"
#include "TrackListHelpers.hpp"
#include "Utils.hpp"

namespace lms::ui::MultisearchListHelpers
{

    std::unique_ptr<Wt::WWidget> createEntry(const db::ObjectPtr<db::Track>& track, PlayQueueController& playQueueController, Filters& filters)
    {
        auto entry = std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Multisearch.template.entry-track"));

        TrackListHelpers::bindName(*entry, track);
        TrackListHelpers::bindArtists(*entry, track);
        TrackListHelpers::bindRelease(*entry, track);
        TrackListHelpers::bindDuration(*entry, track);
        TrackListHelpers::bindPlayQueueController(*entry, track, playQueueController);
        TrackListHelpers::bindStarred(*entry, track);
        TrackListHelpers::bindDownload(*entry, track);
        TrackListHelpers::bindInfo(*entry, track, filters);

        return entry;
    }

    std::unique_ptr<Wt::WWidget> createEntry(const db::ObjectPtr<db::Release>& release, PlayQueueController& playQueueController, Filters&)
    {
        auto entry = std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Multisearch.template.entry-release"));
        entry->addFunction("tr", &Wt::WTemplate::Functions::tr);
        entry->addFunction("id", &Wt::WTemplate::Functions::id);

        releaseListHelpers::bindName(*entry, release);
        releaseListHelpers::bindCover(*entry, release, CoverResource::Size::Small);
        releaseListHelpers::bindArtists(*entry, release);
        releaseListHelpers::bindDuration(*entry, release);
        releaseListHelpers::bindPlayQueueController(*entry, release, playQueueController, true);
        releaseListHelpers::bindStarred(*entry, release);
        releaseListHelpers::bindDownload(*entry, release);
        releaseListHelpers::bindInfo(*entry, release);
        releaseListHelpers::bindMbid(*entry, release);

        return entry;
    }

    std::unique_ptr<Wt::WWidget> createEntry(const db::ObjectPtr<db::Artist>& artist, PlayQueueController&, Filters&)
    {
        auto entry = std::make_unique<Template>(Wt::WString::tr("Lms.Explore.Multisearch.template.entry-artist"));

        ArtistListHelpers::bindName(*entry, artist);
        ArtistListHelpers::bindCover(*entry, artist);

        return entry;
    }
} // namespace lms::ui::MultisearchListHelpers
