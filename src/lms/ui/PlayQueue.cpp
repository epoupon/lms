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
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/User.hpp"
#include "services/feedback/IFeedbackService.hpp"
#include "services/recommendation/IPlaylistGeneratorService.hpp"

#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"
#include "ModalManager.hpp"
#include "State.hpp"
#include "Utils.hpp"
#include "common/InfiniteScrollingContainer.hpp"
#include "common/MandatoryValidator.hpp"
#include "common/ValueStringModel.hpp"
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

                TrackList::find(LmsApp->getDbSession(), params, [&](const TrackList::pointer& trackList) {
                    model->add(Wt::WString::fromUTF8(std::string{ trackList->getName() }), trackList->getId());
                });

                return model;
            }

            std::shared_ptr<TrackListModel> trackListModel{ createTrackListModel() };
        };
    } // namespace

    PlayQueue::PlayQueue()
        : Template{ Wt::WString::tr("Lms.PlayQueue.template") }
        , _capacity{ core::Service<core::IConfig>::get()->getULong("playqueue-max-entry-count", 1000) }
    {
        initTrackLists();

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
        _entriesContainer->onRequestElements.connect([this] {
            addSome();
            updateCurrentTrack(true);
        });

        Wt::WPushButton* shuffleBtn{ bindNew<Wt::WPushButton>("shuffle-btn", Wt::WString::tr("Lms.PlayQueue.template.shuffle-btn"), Wt::TextFormat::XHTML) };
        shuffleBtn->clicked().connect([this] {
            {
                // TODO write scope could be reduced
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                db::TrackList::pointer queue{ getQueue() };
                auto entries{ queue->getEntries().results };
                core::random::shuffleContainer(entries);

                queue.modify()->clear();
                for (const db::TrackListEntry::pointer& entry : entries)
                    LmsApp->getDbSession().create<db::TrackListEntry>(entry->getTrack(), queue);
            }
            _entriesContainer->reset();
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

            const std::size_t trackPos{ state::readValue<size_t>("player_cur_playing_track_pos").value_or(0) };
            loadTrack(trackPos, false);
        });

        LmsApp->preQuit().connect([this] {
            if (LmsApp->getUserType() == db::UserType::DEMO)
            {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                LMS_LOG(UI, DEBUG, "Removing queue (tracklist id " << _queueId.toString() << ")");
                if (db::TrackList::pointer queue{ getQueue() })
                    queue.remove();
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

    db::TrackList::pointer PlayQueue::getQueue() const
    {
        return db::TrackList::find(LmsApp->getDbSession(), _queueId);
    }

    bool PlayQueue::isFull() const
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };
        return getQueue()->getCount() == getCapacity();
    }

    void PlayQueue::clearTracks()
    {
        {
            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };
            getQueue().modify()->clear();
        }

        _entriesContainer->reset();
        _trackPos.reset();
        updateInfo();
    }

    void PlayQueue::stop()
    {
        updateCurrentTrack(false);
        _trackPos.reset();
        _isTrackSelected = false;
        trackUnselected.emit();
    }

    void PlayQueue::loadTrack(std::size_t pos, bool play)
    {
        updateCurrentTrack(false);

        db::TrackId trackId{};
        std::optional<float> replayGain{};
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const db::TrackList::pointer queue{ getQueue() };

            // If out of range, stop playing
            if (pos >= queue->getCount())
            {
                if (!isRepeatAllSet() || queue->getCount() == 0)
                {
                    stop();
                    return;
                }

                pos = 0;
            }

            _trackPos = pos;

            const db::Track::pointer track{ queue->getEntry(*_trackPos)->getTrack() };
            trackId = track->getId();
            replayGain = getReplayGain(pos, track);
        }

        state::writeValue<size_t>("player_cur_playing_track_pos", pos);

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
        if (!_trackPos)
        {
            loadTrack(0, true);
            return;
        }

        loadTrack(*_trackPos + 1, true);
    }

    void PlayQueue::onPlaybackEnded()
    {
        playNext();
    }

    std::size_t PlayQueue::getCount()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };
        return getQueue()->getCount();
    }

    void PlayQueue::initTrackLists()
    {
        auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

        db::TrackList::pointer queue;
        db::TrackList::pointer radioStartingTracks;

        if (LmsApp->getUserType() != db::UserType::DEMO)
        {
            static const std::string queueName{ "__queued_tracks__" };
            queue = db::TrackList::find(LmsApp->getDbSession(), queueName, db::TrackListType::Internal, LmsApp->getUserId());
            if (!queue)
            {
                queue = LmsApp->getDbSession().create<db::TrackList>(queueName, db::TrackListType::Internal);
                queue.modify()->setVisibility(db::TrackList::Visibility::Private);
                queue.modify()->setUser(LmsApp->getUser());
            }
        }
        else
        {
            static const std::string queueName{ "__temp_queue__" };
            queue = LmsApp->getDbSession().create<db::TrackList>(queueName, db::TrackListType::Internal);
            queue.modify()->setVisibility(db::TrackList::Visibility::Private);
            queue.modify()->setUser(LmsApp->getUser());
        }

        _queueId = queue->getId();
    }

    void PlayQueue::updateInfo()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::TrackList::pointer queue{ getQueue() };
        const std::size_t trackCount{ queue->getCount() };
        _nbTracks->setText(Wt::WString::trn("Lms.track-count", trackCount).arg(trackCount));
        _duration->setText(utils::durationToString(queue->getDuration()));
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

    void PlayQueue::enqueueTracks(const std::vector<db::TrackId>& trackIds)
    {
        {
            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

            db::TrackList::pointer queue{ getQueue() };
            const std::size_t queueSize{ queue->getCount() };

            std::size_t nbTracksToEnqueue{ queueSize + trackIds.size() > getCapacity() ? getCapacity() - queueSize : trackIds.size() };
            for (const db::TrackId trackId : trackIds)
            {
                if (nbTracksToEnqueue == 0)
                    break;

                db::Track::pointer track{ db::Track::find(LmsApp->getDbSession(), trackId) };
                if (!track)
                    continue;

                LmsApp->getDbSession().create<db::TrackListEntry>(track, queue);
                nbTracksToEnqueue--;
            }
        }

        updateInfo();
        addSome();
        _entriesContainer->setHasMore();
    }

    std::vector<db::TrackId> PlayQueue::getAndClearNextTracks()
    {
        std::vector<db::TrackId> tracks;

        auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

        db::TrackList::pointer queue{ getQueue() };
        auto entries{ queue->getEntries(db::Range{ _trackPos ? *_trackPos + 1 : 0, getCapacity() }) };
        tracks.reserve(entries.results.size());
        for (db::TrackListEntry::pointer& entry : entries.results)
        {
            tracks.push_back(entry->getTrack()->getId());
            entry.remove();
        }

        if (_trackPos)
        {
            // entries may have been cleared
            if (*_trackPos + 1 < _entriesContainer->getCount())
                _entriesContainer->remove(*_trackPos + 1, _entriesContainer->getCount() - 1);
        }
        else
        {
            _entriesContainer->reset();
        }

        return tracks;
    }

    void PlayQueue::play(const std::vector<db::TrackId>& trackIds)
    {
        playAtIndex(trackIds, 0);
    }

    void PlayQueue::playNext(const std::vector<db::TrackId>& trackIds)
    {
        std::vector<db::TrackId> nextTracks{ getAndClearNextTracks() };
        nextTracks.insert(std::cbegin(nextTracks), std::cbegin(trackIds), std::cend(trackIds));
        playOrAddLast(nextTracks);
    }

    void PlayQueue::playShuffled(const std::vector<db::TrackId>& trackIds)
    {
        clearTracks();
        std::vector<db::TrackId> shuffledTrackIds{ trackIds };
        core::random::shuffleContainer(shuffledTrackIds);
        enqueueTracks(shuffledTrackIds);
        loadTrack(0, true);
    }

    void PlayQueue::playOrAddLast(const std::vector<db::TrackId>& trackIds)
    {
        enqueueTracks(trackIds);
        if (!_isTrackSelected)
            loadTrack(0, true);
    }

    void PlayQueue::playAtIndex(const std::vector<db::TrackId>& trackIds, std::size_t index)
    {
        clearTracks();
        enqueueTracks(trackIds);
        loadTrack(index, true);
    }

    void PlayQueue::addSome()
    {
        auto transaction{ LmsApp->getDbSession().createReadTransaction() };

        const db::TrackList::pointer queue{ getQueue() };
        const auto entries{ queue->getEntries(db::Range{ _entriesContainer->getCount(), _batchSize }) };
        for (const db::TrackListEntry::pointer& tracklistEntry : entries.results)
            addEntry(tracklistEntry);

        _entriesContainer->setHasMore(entries.moreResults);
    }

    void PlayQueue::addEntry(const db::TrackListEntry::pointer& tracklistEntry)
    {
        const db::TrackListEntryId tracklistEntryId{ tracklistEntry->getId() };
        const auto track{ tracklistEntry->getTrack() };
        const db::TrackId trackId{ track->getId() };

        Template* entry{ _entriesContainer->addNew<Template>(Wt::WString::tr("Lms.PlayQueue.template.entry")) };
        entry->addFunction("id", &Wt::WTemplate::Functions::id);

        entry->bindString("name", Wt::WString::fromUTF8(track->getName()), Wt::TextFormat::Plain);

        const auto artists{ track->getArtistIds({ db::TrackArtistLinkType::Artist }) };
        if (!artists.empty())
        {
            entry->setCondition("if-has-artists", true);
            entry->bindWidget("artists", utils::createArtistAnchorList(artists));
            entry->bindWidget("artists-md", utils::createArtistAnchorList(artists));
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

        if (const auto release{ track->getRelease() })
        {
            entry->setCondition("if-has-release", true);
            entry->bindWidget("release", utils::createReleaseAnchor(release));

            Wt::WAnchor* anchor{ entry->bindWidget("cover", utils::createReleaseAnchor(release, false)) };
            image->addStyleClass("Lms-cover-anchor"); // HACK
            anchor->setImage(std::move(image));
        }
        else
        {
            entry->bindWidget("cover", std::move(image));
        }

        entry->bindString("duration", utils::durationToString(track->getDuration()), Wt::TextFormat::Plain);

        Wt::WPushButton* playBtn{ entry->bindNew<Wt::WPushButton>("play-btn", Wt::WString::tr("Lms.template.play-btn"), Wt::TextFormat::XHTML) };
        playBtn->clicked().connect([this, entry] {
            const std::optional<std::size_t> pos{ _entriesContainer->getIndexOf(*entry) };
            if (pos)
                loadTrack(*pos, true);
        });

        Wt::WPushButton* delBtn{ entry->bindNew<Wt::WPushButton>("del-btn", Wt::WString::tr("Lms.template.delete-btn"), Wt::TextFormat::XHTML) };
        delBtn->setToolTip(Wt::WString::tr("Lms.delete"));
        delBtn->clicked().connect([this, tracklistEntryId, entry] {
            // Remove the entry n both the widget tree and the playqueue
            {
                auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

                db::TrackListEntry::pointer entryToRemove{ db::TrackListEntry::getById(LmsApp->getDbSession(), tracklistEntryId) };
                entryToRemove.remove();
            }

            if (_trackPos)
            {
                const std::optional<std::size_t> pos{ _entriesContainer->getIndexOf(*entry) };
                if (pos && *_trackPos >= *pos)
                    (*_trackPos)--;
                else if (*_trackPos >= _entriesContainer->getCount())
                    _trackPos.reset();
            }

            _entriesContainer->remove(*entry);

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

        auto isStarred{ [=] { return core::Service<feedback::IFeedbackService>::get()->isStarred(LmsApp->getUserId(), trackId); } };

        Wt::WPushButton* starBtn{ entry->bindNew<Wt::WPushButton>("star", Wt::WString::tr(isStarred() ? "Lms.Explore.unstar" : "Lms.Explore.star")) };
        starBtn->clicked().connect([=] {
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

        entry->bindNew<Wt::WPushButton>("download", Wt::WString::tr("Lms.Explore.download"))
            ->setLink(Wt::WLink{ std::make_unique<DownloadTrackResource>(trackId) });
    }

    void PlayQueue::enqueueRadioTracksIfNeeded()
    {
        if (!isRadioModeSet())
            return;

        bool addTracks{};
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            const db::TrackList::pointer queue{ getQueue() };

            // If out of range, stop playing
            if (_trackPos >= queue->getCount() - 1)
                addTracks = true;
        }

        if (addTracks)
            enqueueRadioTracks();
    }

    void PlayQueue::enqueueRadioTracks()
    {
        std::vector<db::TrackId> trackIds = core::Service<recommendation::IPlaylistGeneratorService>::get()->extendPlaylist(_queueId, 15);
        enqueueTracks(trackIds);
    }

    std::optional<float> PlayQueue::getReplayGain(std::size_t pos, const db::Track::pointer& track) const
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
            gain = track->getTrackReplayGain();
            break;

        case MediaPlayer::Settings::ReplayGain::Mode::Release:
            gain = track->getReleaseReplayGain();
            if (!gain)
                gain = track->getTrackReplayGain();
            break;

        case MediaPlayer::Settings::ReplayGain::Mode::Auto:
            {
                const db::TrackList::pointer queue{ getQueue() };
                const db::TrackListEntry::pointer prevEntry{ pos > 0 ? queue->getEntry(pos - 1) : db::TrackListEntry::pointer{} };
                const db::TrackListEntry::pointer nextEntry{ queue->getEntry(pos + 1) };
                const db::Track::pointer prevTrack{ prevEntry ? prevEntry->getTrack() : db::Track::pointer{} };
                const db::Track::pointer nextTrack{ nextEntry ? nextEntry->getTrack() : db::Track::pointer{} };

                if ((prevTrack && prevTrack->getRelease() && prevTrack->getRelease() == track->getRelease())
                    || (nextTrack && nextTrack->getRelease() && nextTrack->getRelease() == track->getRelease()))
                {
                    gain = track->getReleaseReplayGain();
                    if (!gain)
                        gain = track->getTrackReplayGain();
                }
                else
                {
                    gain = track->getTrackReplayGain();
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
            Session& session{ LmsApp->getDbSession() };
            auto transaction{ session.createWriteTransaction() };
            TrackList::pointer trackList{ session.create<TrackList>(name.toUTF8(), TrackListType::PlayList) };
            trackList.modify()->setVisibility(TrackList::Visibility::Private);
            trackList.modify()->setUser(LmsApp->getUser());
            trackListId = trackList->getId();
        }

        exportToTrackList(trackListId);
    }

    void PlayQueue::exportToTrackList(db::TrackListId trackListId)
    {
        using namespace db;

        Session& session{ LmsApp->getDbSession() };
        auto transaction{ session.createWriteTransaction() };

        TrackList::pointer trackList{ TrackList::find(LmsApp->getDbSession(), trackListId) };
        trackList.modify()->clear();
        trackList.modify()->setLastModifiedDateTime(Wt::WDateTime::currentDateTime());

        Track::FindParameters params;
        params.setTrackList(_queueId);
        params.setSortMethod(TrackSortMethod::TrackList);

        Track::find(session, params, [&](const Track::pointer& track) {
            session.create<TrackListEntry>(track, trackList);
        });
    }
} // namespace lms::ui
