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

#include "PlayQueue.hpp"

#include <array>

#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WRadioButton.h>
#include <Wt/WStackedWidget.h>
#include <Wt/WTemplateFormView.h>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Random.hpp"
#include "core/Service.hpp"
#include "database/ISession.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/PlayQueue.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/recommendation/IRecommendationService.hpp"

#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"
#include "ModalManager.hpp"
#include "State.hpp"
#include "Utils.hpp"
#include "common/InfiniteScrollingContainer.hpp"
#include "common/MandatoryValidator.hpp"
#include "common/ValueStringModel.hpp"
#include "explore/TrackListHelpers.hpp"
#include "resource/DownloadResource.hpp"

namespace lms::ui
{
    namespace
    {
        class CreateTrackListModel : public Wt::WFormModel
        {
        public:
            static inline const Field NameField{ "name" };

            CreateTrackListModel()
            {
                addField(NameField);
                setValidator(NameField, createMandatoryValidator());
            }

            Wt::WString getName() const { return valueText(NameField); }
        };

        class ReplaceTrackListModel : public Wt::WFormModel
        {
        public:
            static inline const Field NameField{ "name" };
            using TrackListModel = ValueStringModel<db::TrackListId>;

            ReplaceTrackListModel()
            {
                addField(NameField);
                setValidator(NameField, createMandatoryValidator());
            }

            db::TrackListId getTrackListId() const
            {
                auto row{ trackListModel->getRowFromString(valueText(NameField)) };
                return trackListModel->getValue(*row);
            }

            static std::shared_ptr<TrackListModel> createTrackListModel()
            {
                using namespace db;

                auto model{ std::make_shared<TrackListModel>() };

                auto transaction{ LmsApp->getDbSession().createReadTransaction() };

                TrackList::FindParameters params;
                params.setType(TrackListType::PlayList);
                params.setUser(LmsApp->getUserId());
                params.setSortMethod(TrackListSortMethod::Name);

                TrackList::find(LmsApp->getDbSession(), params, [&](const TrackList& trackList) {
                    model->add(Wt::WString::fromUTF8(std::string{ trackList.getName() }), trackList.getId());
                });

                return model;
            }

            std::shared_ptr<TrackListModel> trackListModel{ createTrackListModel() };
        };
    } // namespace

    PlayQueue::PlayQueue(Filters& filters)
        : Template{ Wt::WString::tr("Lms.PlayQueue.template") }
        , _capacity{ core::Service<core::IConfig>::get()->getULong("ui-playqueue-max-entry-count", 1000) }
        , _filters{ filters }
    {
        initPlayQueue();

        addFunction("id", &Wt::WTemplate::Functions::id);
        addFunction("tr", &Wt::WTemplate::Functions::tr);

        Wt::WPushButton* clearBtn{ bindNew<Wt::WPushButton>("clear-btn", Wt::WString::tr("Lms.PlayQueue.template.clear-btn"), Wt::TextFormat::XHTML) };
        clearBtn->clicked().connect([this] {
            clearTracks();
        });

        Wt::WPushButton* saveBtn{ bindNew<Wt::WPushButton>("save-btn", Wt::WString::tr("Lms.PlayQueue.template.save-btn"), Wt::TextFormat::XHTML) };
        saveBtn->clicked().connect([this] {
            saveAsTrackList();
        });

        _entriesContainer = bindNew<InfiniteScrollingContainer>("entries", Wt::WString::tr("Lms.PlayQueue.template.entry-container"));
        _entriesContainer->setNoResultsMessage(Wt::WString::tr("Lms.PlayQueue.queue-is-empty"));
        _entriesContainer->onRequestElements.connect([this] {
            addSome();
            updateCurrentTrack(true);
        });

        Wt::WPushButton* shuffleBtn{ bindNew<Wt::WPushButton>("shuffle-btn", Wt::WString::tr("Lms.PlayQueue.template.shuffle-btn"), Wt::TextFormat::XHTML) };
        shuffleBtn->clicked().connect([this] {
            {
                // TODO write scope could be reduced
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                std::vector<db::TrackId> trackIds{ db::Track::findIds(LmsApp->getDbSession(), db::Track::FindParameters{}.setTrackList(_trackListId)) };
                core::random::shuffleContainer(trackIds);

                db::TrackList::clear(LmsApp->getDbSession(), _trackListId);
                for (const db::TrackId trackId : trackIds)
                    db::TrackListEntry::create(LmsApp->getDbSession(), trackId, _trackListId);
            }
            _entriesContainer->reset();
            _nextPlayPos.reset();
            addSome();
        });

        _repeatBtn = bindNew<Wt::WCheckBox>("repeat-btn");
        _repeatBtn->clicked().connect([this] {
            state::writeValue<bool>("player_repeat_all", isRepeatAllSet());
        });
        if (state::readValue<bool>("player_repeat_all").value_or(false))
            _repeatBtn->setCheckState(Wt::CheckState::Checked);

        _radioBtn = bindNew<Wt::WCheckBox>("radio-btn");
        _radioBtn->clicked().connect([this] {
            {
                state::writeValue<bool>("player_radio_mode", isRadioModeSet());
            }
            if (isRadioModeSet())
                enqueueRadioTracksIfNeeded();
        });

        if (state::readValue<bool>("player_radio_mode").value_or(false))
        {
            _radioBtn->setCheckState(Wt::CheckState::Checked);
            enqueueRadioTracksIfNeeded();
        }

        _nbTracks = bindNew<Wt::WText>("track-count");
        _duration = bindNew<Wt::WText>("duration");

        LmsApp->getMediaPlayer().settingsLoaded.connect([this] {
            if (_mediaPlayerSettingsLoaded)
                return;

            _mediaPlayerSettingsLoaded = true;

            std::size_t trackPos{};
            {
                auto transaction{ LmsApp->getDbSession().createReadTransaction() };
                if (const std::optional<db::PlayQueue> playQueue{ db::PlayQueue::find(LmsApp->getDbSession(), _playQueueId) })
                    trackPos = playQueue->getCurrentIndex();
            }
            loadTrack(trackPos, false);
        });

        LmsApp->preQuit().connect([this] {
            if (LmsApp->getUserType() == db::UserType::DEMO)
            {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                LMS_LOG(UI, DEBUG, "Removing play queue " << _playQueueId.toString());
                db::PlayQueue::destroy(LmsApp->getDbSession(), std::span{ &_playQueueId, 1 });
            }
        });

        updateInfo();
    }

    bool PlayQueue::isRepeatAllSet() const
    {
        return _repeatBtn->checkState() == Wt::CheckState::Checked;
    }

    bool PlayQueue::isRadioModeSet() const
    {
        return _radioBtn->checkState() == Wt::CheckState::Checked;
    }

    std::vector<db::TrackListEntry> PlayQueue::getEntries(db::Range range) const
    {
        std::vector<db::TrackListEntry> entries;
        db::TrackListEntry::find(LmsApp->getDbSession(), db::TrackListEntry::FindParameters{}.setTrackList(_trackListId).setRange(range), [&](const db::TrackListEntry& entry) {
            entries.push_back(entry);
        });
        return entries;
    }

    std::optional<db::TrackListEntry> PlayQueue::getEntry(std::size_t pos) const
    {
        const auto entries{ getEntries(db::Range{ pos, 1 }) };
        return entries.empty() ? std::nullopt : std::make_optional(entries.front());
    }

    bool PlayQueue::isFull() const
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };
        return db::TrackList::getTrackCount(LmsApp->getDbSession(), _trackListId) == getCapacity();
    }

    void PlayQueue::clearTracks()
    {
        {
            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };
            db::TrackList::clear(LmsApp->getDbSession(), _trackListId);
        }

        _entriesContainer->reset();
        _trackPos.reset();
        _nextPlayPos.reset();
        updateInfo();
    }

    void PlayQueue::stop()
    {
        updateCurrentTrack(false);
        _trackPos.reset();
        _nextPlayPos.reset();
        _isTrackSelected = false;
        trackUnselected.emit();
    }

    void PlayQueue::loadTrack(std::size_t pos, bool play, ResetNextPlayPos resetNextPlayPos)
    {
        updateCurrentTrack(false);

        db::TrackId trackId{};
        std::optional<float> replayGain{};
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const std::size_t queueSize{ db::TrackList::getTrackCount(LmsApp->getDbSession(), _trackListId) };

            // If out of range, stop playing
            if (pos >= queueSize)
            {
                if (!isRepeatAllSet() || queueSize == 0)
                {
                    stop();
                    return;
                }

                pos = 0;
            }

            _trackPos = pos;
            if (resetNextPlayPos.value())
                _nextPlayPos.reset();

            const std::optional<db::Track> track{ db::Track::find(LmsApp->getDbSession(), getEntry(*_trackPos)->getTrackId()) };
            trackId = track->getId();
            replayGain = getReplayGain(pos, *track);
        }

        saveCurrentIndex(pos);

        enqueueRadioTracksIfNeeded();
        updateCurrentTrack(true);
        _isTrackSelected = true;
        trackSelected.emit(trackId, play, replayGain ? *replayGain : 0);
    }

    void PlayQueue::playPrevious()
    {
        if (!_trackPos)
            return;

        if (*_trackPos == 0)
            stop();
        else
            loadTrack(*_trackPos - 1, true);
    }

    void PlayQueue::playNext()
    {
        advanceTrack(ResetNextPlayPos{ true });
    }

    void PlayQueue::onPlaybackEnded()
    {
        advanceTrack(ResetNextPlayPos{ false });
    }

    void PlayQueue::advanceTrack(ResetNextPlayPos resetNextPlayPos)
    {
        if (!_trackPos)
        {
            loadTrack(0, true, resetNextPlayPos);
            return;
        }

        loadTrack(*_trackPos + 1, true, resetNextPlayPos);
    }

    std::size_t PlayQueue::getCount()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };
        return db::TrackList::getTrackCount(LmsApp->getDbSession(), _trackListId);
    }

    void PlayQueue::initPlayQueue()
    {
        auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

        std::optional<db::PlayQueue> playQueue;

        // Demo sessions share the same user: each one gets its own play queue
        if (LmsApp->getUserType() != db::UserType::DEMO)
            playQueue = db::PlayQueue::find(LmsApp->getDbSession(), LmsApp->getUserId(), db::PlayQueueType::UI);

        if (!playQueue)
            playQueue = db::PlayQueue::create(LmsApp->getDbSession(), LmsApp->getUserId(), db::PlayQueueType::UI);

        _playQueueId = playQueue->getId();
        _trackListId = playQueue->getTrackListId();
    }

    void PlayQueue::saveCurrentIndex(std::size_t index)
    {
        auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

        std::optional<db::PlayQueue> playQueue{ db::PlayQueue::find(LmsApp->getDbSession(), _playQueueId) };
        if (!playQueue || playQueue->getCurrentIndex() == index)
            return;

        playQueue->setCurrentIndex(index);
        db::PlayQueue::update(LmsApp->getDbSession(), *playQueue);
    }

    void PlayQueue::updateInfo()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const std::size_t trackCount{ db::TrackList::getTrackCount(LmsApp->getDbSession(), _trackListId) };
        _nbTracks->setText(Wt::WString::trn("Lms.track-count", trackCount).arg(trackCount));
        _duration->setText(utils::durationToString(db::TrackList::getDuration(LmsApp->getDbSession(), _trackListId)));
        trackCountChanged.emit(trackCount);
    }

    void PlayQueue::updateCurrentTrack(bool selected)
    {
        if (!_trackPos || *_trackPos >= static_cast<std::size_t>(_entriesContainer->getCount()))
            return;

        Template* entry{ static_cast<Template*>(_entriesContainer->getWidget(*_trackPos)) };
        if (!entry)
            return;

        entry->toggleStyleClass("Lms-entry-playing", selected);
    }

    void PlayQueue::enqueueTracks(std::span<const db::TrackId> trackIds)
    {
        {
            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

            const std::size_t queueSize{ db::TrackList::getTrackCount(LmsApp->getDbSession(), _trackListId) };

            std::size_t nbTracksToEnqueue{ queueSize + trackIds.size() > getCapacity() ? getCapacity() - queueSize : trackIds.size() };
            for (const db::TrackId trackId : trackIds)
            {
                if (nbTracksToEnqueue == 0)
                    break;

                std::optional<db::Track> track{ db::Track::find(LmsApp->getDbSession(), trackId) };
                if (!track)
                    continue;

                db::TrackListEntry::create(LmsApp->getDbSession(), track->getId(), _trackListId);
                nbTracksToEnqueue--;
            }
        }

        updateInfo();
        addSome();
        _entriesContainer->setHasMore();
    }

    std::vector<db::TrackId> PlayQueue::getAndClearTracksFrom(std::size_t pos)
    {
        std::vector<db::TrackId> tracks;

        auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

        const auto entries{ getEntries(db::Range{ pos, getCapacity() }) };
        tracks.reserve(entries.size());
        std::vector<db::TrackListEntryId> entryIds;
        entryIds.reserve(entries.size());
        for (const db::TrackListEntry& entry : entries)
        {
            tracks.push_back(entry.getTrackId());
            entryIds.push_back(entry.getId());
        }
        db::TrackListEntry::destroy(LmsApp->getDbSession(), entryIds);

        if (pos == 0)
        {
            _entriesContainer->reset();
        }
        else if (pos < _entriesContainer->getCount())
        {
            // entries may have been cleared
            _entriesContainer->remove(pos, _entriesContainer->getCount() - 1);
        }

        return tracks;
    }

    void PlayQueue::play(std::span<const db::TrackId> trackIds)
    {
        playAtIndex(trackIds, 0);
    }

    void PlayQueue::playNext(std::span<const db::TrackId> trackIds)
    {
        const std::size_t defaultInsertPos{ _trackPos ? *_trackPos + 1 : 0 };
        if (!_nextPlayPos || *_nextPlayPos < defaultInsertPos)
            _nextPlayPos = defaultInsertPos;

        const std::size_t insertPos{ *_nextPlayPos };

        std::vector<db::TrackId> tracksToInsert{ getAndClearTracksFrom(insertPos) };
        tracksToInsert.insert(std::cbegin(tracksToInsert), std::cbegin(trackIds), std::cend(trackIds));

        playOrAddLast(tracksToInsert);

        // set after playOrAddLast: it may call loadTrack() (queue was empty), which resets _nextPlayPos
        _nextPlayPos = insertPos + trackIds.size();
    }

    void PlayQueue::playShuffled(std::span<const db::TrackId> trackIds)
    {
        clearTracks();
        std::vector<db::TrackId> shuffledTrackIds{ std::cbegin(trackIds), std::cend(trackIds) };
        core::random::shuffleContainer(shuffledTrackIds);
        enqueueTracks(shuffledTrackIds);
        loadTrack(0, true);
    }

    void PlayQueue::playOrAddLast(std::span<const db::TrackId> trackIds)
    {
        enqueueTracks(trackIds);
        if (!_isTrackSelected)
            loadTrack(0, true);
    }

    void PlayQueue::playAtIndex(std::span<const db::TrackId> trackIds, std::size_t index)
    {
        clearTracks();
        enqueueTracks(trackIds);
        loadTrack(index, true);
    }

    void PlayQueue::addSome()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const auto entries{ getEntries(db::Range{ static_cast<std::size_t>(_entriesContainer->getCount()), _batchSize }) };
        for (const db::TrackListEntry& tracklistEntry : entries)
            addEntry(tracklistEntry);

        _entriesContainer->setHasMore(entries.size() == _batchSize);
    }

    void PlayQueue::addEntry(const db::TrackListEntry& tracklistEntry)
    {
        const db::TrackListEntryId tracklistEntryId{ tracklistEntry.getId() };
        const std::optional<db::Track> track{ db::Track::find(LmsApp->getDbSession(), tracklistEntry.getTrackId()) };
        const db::TrackId trackId{ track->getId() };

        const utils::TrackDisplayInfo displayInfo{ utils::computeTrackDisplayInfo(*track) };

        Template* entry{ _entriesContainer->addNew<Template>(Wt::WString::tr("Lms.PlayQueue.template.entry")) };
        entry->addFunction("id", &Wt::WTemplate::Functions::id);

        entry->bindString("name", Wt::WString::fromUTF8(displayInfo.title), Wt::TextFormat::Plain);
        if (displayInfo.workName)
        {
            entry->setCondition("if-has-work", true);
            entry->bindString("work", Wt::WString::fromUTF8(*displayInfo.workName), Wt::TextFormat::Plain);
        }

        const auto artists{ db::Artist::findIds(LmsApp->getDbSession(), db::Artist::FindParameters{}.setTrack(trackId).setTrackArtistLinkType(db::TrackArtistLinkType::Artist)) };
        if (!artists.empty())
        {
            entry->setCondition("if-has-artists", true);
            entry->bindWidget("artists", utils::createArtistsAnchors(*track, db::TrackArtistLinkType::Artist));
            entry->bindWidget("artists-md", utils::createArtistsAnchors(*track, db::TrackArtistLinkType::Artist));
        }

        db::ArtworkId artworkId{ track->getPreferredMediaArtworkId() };
        if (!artworkId.isValid())
            artworkId = track->getPreferredArtworkId();

        std::unique_ptr<Wt::WImage> image;
        if (artworkId.isValid())
            image = utils::createArtworkImage(artworkId, ArtworkResource::DefaultArtworkType::Track, ArtworkResource::Size::Small);
        else
            image = utils::createDefaultArtworkImage(ArtworkResource::DefaultArtworkType::Track);

        image->addStyleClass("Lms-cover-track rounded"); // HACK

        if (const std::optional<db::Release> release{ db::Release::find(LmsApp->getDbSession(), track->getReleaseId()) })
        {
            entry->setCondition("if-has-release", true);
            entry->bindWidget("release", utils::createReleaseAnchor(*release));

            Wt::WAnchor* anchor{ entry->bindWidget("cover", utils::createReleaseAnchor(*release, false)) };
            image->addStyleClass("Lms-cover-anchor"); // HACK
            anchor->setImage(std::move(image));
        }
        else
        {
            entry->bindWidget("cover", std::move(image));
        }

        entry->bindString("duration", utils::durationToString(track->getDuration()), Wt::TextFormat::Plain);

        Wt::WPushButton* playBtn{ entry->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.template.play-btn"), Wt::TextFormat::XHTML) };
        playBtn->setAttributeValue("aria-label", Wt::WString::tr("Lms.play-item").arg(displayInfo.title));
        playBtn->clicked().connect([this, entry] {
            const std::optional<std::size_t> pos{ _entriesContainer->getIndexOf(*entry) };
            if (pos)
                loadTrack(*pos, true);
        });

        Wt::WPushButton* delBtn{ entry->bindNew<Wt::WPushButton>("del-btn", Wt::WString::tr("Lms.template.delete-btn"), Wt::TextFormat::XHTML) };
        delBtn->setAttributeValue("aria-label", Wt::WString::tr("Lms.delete-item").arg(displayInfo.title));
        delBtn->setToolTip(Wt::WString::tr("Lms.delete"));
        delBtn->clicked().connect([this, tracklistEntryId, entry] {
            // Remove the entry n both the widget tree and the playqueue
            {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                db::TrackListEntry::destroy(LmsApp->getDbSession(), std::array{ tracklistEntryId });
            }

            if (_trackPos)
            {
                const std::optional<std::size_t> pos{ _entriesContainer->getIndexOf(*entry) };
                if (pos && *_trackPos >= *pos)
                    (*_trackPos)--;
                else if (*_trackPos >= _entriesContainer->getCount())
                    _trackPos.reset();
            }
            _nextPlayPos.reset();

            _entriesContainer->remove(*entry);
            if (_entriesContainer->getCount() == 0)
                _entriesContainer->reset(); // re-fetch from the top: entries beyond the loaded batch may remain

            updateInfo();
        });

        entry->bindNew<Wt::WPushButton>("more-btn", Wt::WString::tr("Lms.template.more-btn"), Wt::TextFormat::XHTML);
        entry->bindNew<Wt::WPushButton>("play", Wt::WString::tr("Lms.Explore.play"))
            ->clicked()
            .connect([this, entry] {
                const std::optional<std::size_t> pos{ _entriesContainer->getIndexOf(*entry) };
                if (pos)
                    loadTrack(*pos, true);
            });

        auto hasFeedback{ [=] { return core::Service<feedback::IFeedbackService>::get()->getFeedback(LmsApp->getUserId(), trackId) == db::FeedbackValue::Loved; } };

        Wt::WPushButton* feedbackBtn{ entry->bindNew<Wt::WPushButton>("star", Wt::WString::tr(hasFeedback() ? "Lms.Explore.unstar" : "Lms.Explore.star")) };
        feedbackBtn->clicked().connect([=] {
            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

            if (hasFeedback())
            {
                core::Service<feedback::IFeedbackService>::get()->setFeedback(LmsApp->getUserId(), trackId, db::FeedbackValue::None);
                feedbackBtn->setText(Wt::WString::tr("Lms.Explore.star"));
            }
            else
            {
                core::Service<feedback::IFeedbackService>::get()->setFeedback(LmsApp->getUserId(), trackId, db::FeedbackValue::Loved);
                feedbackBtn->setText(Wt::WString::tr("Lms.Explore.unstar"));
            }
        });

        if (LmsApp->areDownloadsEnabled())
        {
            entry->setCondition("if-has-download", true);
            entry->bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
                ->setLink(Wt::WLink{ std::make_unique<DownloadTrackResource>(trackId) });
        }

        entry->bindNew<Wt::WPushButton>("track-info", Wt::WString::tr("Lms.Explore.track-info"))
            ->clicked()
            .connect([this, trackId] { trackListHelpers::showTrackInfoModal(trackId, _filters); });
    }

    void PlayQueue::enqueueRadioTracksIfNeeded()
    {
        if (!isRadioModeSet())
            return;

        bool addTracks{};
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            // If out of range, stop playing
            if (_trackPos >= db::TrackList::getTrackCount(LmsApp->getDbSession(), _trackListId) - 1)
                addTracks = true;
        }

        if (addTracks)
            enqueueRadioTracks();
    }

    void PlayQueue::enqueueRadioTracks()
    {
        const recommendation::TrackResults results{ core::Service<recommendation::IRecommendationService>::get()->findSimilarTracks(_trackListId, 15) };

        std::vector<db::TrackId> trackIds;
        trackIds.reserve(results.size());
        for (const auto& result : results)
            trackIds.push_back(result.id);

        enqueueTracks(trackIds);
    }

    std::optional<float> PlayQueue::getReplayGain(std::size_t pos, const db::Track& track) const
    {
        const auto& settings{ LmsApp->getMediaPlayer().getSettings() };
        if (!settings)
            return std::nullopt;

        std::optional<MediaPlayer::Gain> gain;

        switch (settings->replayGain.mode)
        {
        case MediaPlayer::Settings::ReplayGain::Mode::None:
            return std::nullopt;

        case MediaPlayer::Settings::ReplayGain::Mode::Track:
            gain = track.getReplayGain();
            break;

        case MediaPlayer::Settings::ReplayGain::Mode::Release:
            {
                const auto medium{ db::Medium::find(LmsApp->getDbSession(), track.getMediumId()) };
                if (medium && medium->getReplayGain())
                    gain = medium->getReplayGain();

                if (!gain)
                    gain = track.getReplayGain();

                break;
            }

        case MediaPlayer::Settings::ReplayGain::Mode::Auto:
            {
                const auto getReleaseIdOfEntry{ [&](const std::optional<db::TrackListEntry>& entry) {
                    const std::optional<db::Track> entryTrack{ entry ? db::Track::find(LmsApp->getDbSession(), entry->getTrackId()) : std::nullopt };
                    return entryTrack ? entryTrack->getReleaseId() : db::ReleaseId{};
                } };
                const db::ReleaseId prevReleaseId{ getReleaseIdOfEntry(pos > 0 ? getEntry(pos - 1) : std::nullopt) };
                const db::ReleaseId nextReleaseId{ getReleaseIdOfEntry(getEntry(pos + 1)) };

                if ((prevReleaseId.isValid() && prevReleaseId == track.getReleaseId())
                    || (nextReleaseId.isValid() && nextReleaseId == track.getReleaseId()))
                {
                    const auto medium{ db::Medium::find(LmsApp->getDbSession(), track.getMediumId()) };
                    if (medium && medium->getReplayGain())
                        gain = medium->getReplayGain();
                    if (!gain)
                        gain = track.getReplayGain();
                }
                else
                {
                    gain = track.getReplayGain();
                }
                break;
            }
        }

        if (gain)
            return *gain + settings->replayGain.preAmpGain;

        return settings->replayGain.preAmpGainIfNoInfo;
    }

    void PlayQueue::saveAsTrackList()
    {
        auto modal{ std::make_unique<Template>(Wt::WString::tr("Lms.PlayQueue.template.save-as-tracklist")) };
        modal->addFunction("id", &Wt::WTemplate::Functions::id);
        modal->addFunction("tr", &Wt::WTemplate::Functions::tr);
        Wt::WWidget* modalPtr{ modal.get() };

        auto* cancelBtn{ modal->bindNew<Wt::WPushButton>("cancel-btn", Wt::WString::tr("Lms.cancel")) };
        cancelBtn->clicked().connect([=] {
            LmsApp->getModalManager().dispose(modalPtr);
        });

        Wt::WStackedWidget* contentStack{ modal->bindNew<Wt::WStackedWidget>("contents") };

        // Create/Replace selector
        enum Index : int
        {
            CreateTrackList = 0,
            ReplaceTrackList = 1,
        };
        {
            modal->bindNew<Wt::WRadioButton>("replace-tracklist-btn")
                ->clicked()
                .connect([=] {
                    contentStack->setCurrentIndex(Index::ReplaceTrackList);
                });

            Wt::WRadioButton* createTrackListBtn{ modal->bindNew<Wt::WRadioButton>("create-tracklist-btn") };
            createTrackListBtn->setChecked();
            createTrackListBtn->clicked().connect([=] {
                contentStack->setCurrentIndex(Index::CreateTrackList);
            });
        }

        // Create TrackList
        Wt::WTemplateFormView* createTrackList{ contentStack->addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.PlayQueue.template.save-as-tracklist.create-tracklist")) };
        auto createTrackListModel{ std::make_shared<CreateTrackListModel>() };
        createTrackList->setFormWidget(CreateTrackListModel::NameField, std::make_unique<Wt::WLineEdit>());
        createTrackList->updateView(createTrackListModel.get());

        // Replace TrackList
        Wt::WTemplateFormView* replaceTrackList{ contentStack->addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.PlayQueue.template.save-as-tracklist.replace-tracklist")) };
        auto replaceTrackListModel{ std::make_shared<ReplaceTrackListModel>() };
        {
            auto name{ std::make_unique<Wt::WComboBox>() };
            name->setModel(replaceTrackListModel->trackListModel);
            replaceTrackList->setFormWidget(ReplaceTrackListModel::NameField, std::move(name));
        }
        replaceTrackList->updateView(replaceTrackListModel.get());

        auto* saveBtn{ modal->bindNew<Wt::WPushButton>("save-btn", Wt::WString::tr("Lms.save")) };
        saveBtn->clicked().connect([=, this] {
            bool success{};
            switch (contentStack->currentIndex())
            {
            case Index::CreateTrackList:
                createTrackList->updateModel(createTrackListModel.get());
                if (createTrackListModel->validate())
                {
                    exportToNewTrackList(createTrackListModel->getName());
                    success = true;
                }
                createTrackList->updateView(createTrackListModel.get());
                break;

            case Index::ReplaceTrackList:
                replaceTrackList->updateModel(replaceTrackListModel.get());
                if (replaceTrackListModel->validate())
                {
                    exportToTrackList(replaceTrackListModel->getTrackListId());
                    success = true;
                }
                replaceTrackList->updateView(replaceTrackListModel.get());
                break;
            }

            if (success)
                LmsApp->getModalManager().dispose(modalPtr);
        });

        LmsApp->getModalManager().show(std::move(modal));
    }

    void PlayQueue::exportToNewTrackList(const Wt::WString& name)
    {
        using namespace db;

        db::TrackListId trackListId;

        {
            ISession& session{ LmsApp->getDbSession() };
            auto transaction{ session.createWriteTransaction() };
            TrackList trackList{ TrackList::create(session, name.toUTF8(), TrackListType::PlayList) };
            trackList.setVisibility(TrackList::Visibility::Private);
            trackList.setUser(LmsApp->getUserId());
            TrackList::update(session, trackList);
            trackListId = trackList.getId();
        }

        exportToTrackList(trackListId);
    }

    void PlayQueue::exportToTrackList(db::TrackListId trackListId)
    {
        using namespace db;

        ISession& session{ LmsApp->getDbSession() };
        auto transaction{ session.createWriteTransaction() };

        std::optional<TrackList> trackList{ TrackList::find(session, trackListId) };
        TrackList::clear(session, trackListId);
        trackList->setLastModifiedDateTime(Wt::WDateTime::currentDateTime());
        TrackList::update(session, *trackList);

        for (const TrackId trackId : Track::findIds(session, Track::FindParameters{}.setTrackList(_trackListId)))
            TrackListEntry::create(session, trackId, trackListId);
    }
} // namespace lms::ui
