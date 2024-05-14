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

#include "MultisearchView.hpp"

#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>

#include "database/Session.hpp"
#include "database/Track.hpp"
#include "core/ILogger.hpp"

#include "common/InfiniteScrollingContainer.hpp"
#include "explore/Filters.hpp"
#include "explore/PlayQueueController.hpp"
#include "explore/TrackListHelpers.hpp"
#include "LmsApplication.hpp"

namespace lms::ui
{
    using namespace db;

    Multisearch::Multisearch(Filters& filters, PlayQueueController& playQueueController, Wt::WLineEdit& searEdit)
        : Template{ Wt::WString::tr("Lms.Explore.Multisearch.template") }
        , _filters{ filters }
        , _playQueueController{ playQueueController }
        , _multisearchCollector{ filters, DatabaseCollectorBase::Mode::All,  _maxCount }
    {
        addFunction("tr", &Wt::WTemplate::Functions::tr);
        addFunction("id", &Wt::WTemplate::Functions::id);

        searEdit.setPlaceholderText(Wt::WString::tr("Lms.Explore.Search.search-placeholder")); // TODO: perhaps should come via constructor?
        searEdit.textInput().connect([this, &searEdit]
            {
                if (wApp->internalPath() != "/multisearch") {
                    wApp->setInternalPath("/multisearch", true);
                }
                refreshView(searEdit.text());
            });

        _container = bindNew<InfiniteScrollingContainer>("multisearch-results", Wt::WString::tr("Lms.Explore.Multisearch.template.entry-container"));
        _container->onRequestElements.connect([this]
            {
                addSome();
            });

        filters.updated().connect([this]
            {
                refreshView();
            });

        refreshView();
    }

    void Multisearch::refreshView()
    {
        _container->reset();
    }

    void Multisearch::refreshView(const Wt::WString& searchText)
    {
        _multisearchCollector.setSearch(searchText.toUTF8());
        refreshView();
    }

    void Multisearch::addSome()
    {
        const auto [_, results, moreResults] = _multisearchCollector.get(Range {_container->getCount(), _batchSize});

        auto transaction = LmsApp->getDbSession().createReadTransaction();
        for (const TrackId trackId : results)
        {
            if (const Track::pointer track = Track::find(LmsApp->getDbSession(), trackId))
                _container->add(TrackListHelpers::createEntry(track, _playQueueController, _filters));
        }

        _container->setHasMore(moreResults);
    }
} // namespace lms::ui
