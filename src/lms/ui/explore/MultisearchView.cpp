#include "MultisearchView.hpp"

#include <database/Artist.hpp>
#include <database/Release.hpp>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>

#include "ArtistListHelpers.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "core/ILogger.hpp"

#include "common/InfiniteScrollingContainer.hpp"
#include "explore/Filters.hpp"
#include "explore/TrackListHelpers.hpp"
#include "LmsApplication.hpp"
#include "MultisearchListHelpers.hpp"
#include "ReleaseHelpers.hpp"

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

        searEdit.setPlaceholderText(Wt::WString::tr("Lms.Explore.Search.search-placeholder"));
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

    namespace {
        template<typename IdT>
        void findAndAdd(const IdT& mediumId, Filters& filters, PlayQueueController& playQueueController, InfiniteScrollingContainer& container) {
            if (const auto result = IdT::Target::find(LmsApp->getDbSession(), mediumId))
                container.add(MultisearchListHelpers::createEntry(result, playQueueController, filters));
        }
    }

    void Multisearch::addSome()
    {
        const auto [_, results, moreResults] = _multisearchCollector.get(Range {_container->getCount(), _batchSize});

        auto transaction = LmsApp->getDbSession().createReadTransaction();
        for (const auto mediumId : results)
        {
            std::visit([this](auto&& mediumId){ findAndAdd(mediumId, _filters, _playQueueController, *_container); }, mediumId);

        }

        _container->setHasMore(moreResults);
    }
} // namespace lms::ui
