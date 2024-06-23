#pragma once

#include "database/Types.hpp"

#include "MultisearchCollector.hpp"
#include "common/Template.hpp"

namespace lms::ui
{
    class Filters;
    class InfiniteScrollingContainer;
    class PlayQueueController;

    class Multisearch : public Template
    {
    public:
        Multisearch(Filters& filters, PlayQueueController& playQueueController, Wt::WLineEdit& searEdit);

    private:
        void refreshView();
        void refreshView(const Wt::WString& searchText);
        void addSome();

        static constexpr std::size_t _batchSize{ 6 };
        static constexpr std::size_t _maxCount{ 8000 };

        Filters& _filters;
        PlayQueueController& _playQueueController;
        InfiniteScrollingContainer* _container{};
        MultisearchCollector _multisearchCollector;
        std::shared_ptr<Wt::WButtonGroup> _mediaTypeFilters;
    };
} // namespace lms::ui
