#pragma once


#include <memory>

#include <Wt/WWidget.h>
#include "database/Object.hpp"
#include "database/TrackId.hpp"

namespace lms::db
{
    class Artist;
    class Release;
    class Track;
}

namespace lms::ui
{
    class PlayQueueController;
    class Filters;
}

namespace lms::ui::MultisearchListHelpers
{
    std::unique_ptr<Wt::WWidget> createEntry(const db::ObjectPtr<db::Track>&, PlayQueueController&, Filters&);
    std::unique_ptr<Wt::WWidget> createEntry(const db::ObjectPtr<db::Release>&, PlayQueueController&, Filters&);
    std::unique_ptr<Wt::WWidget> createEntry(const db::ObjectPtr<db::Artist>&, PlayQueueController&, Filters&);
} // namespace lms::ui

