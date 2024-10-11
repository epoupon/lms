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

#include "SettingsView.hpp"

#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WDoubleSpinBox.h>
#include <Wt/WFormModel.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WTemplateFormView.h>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/Service.hpp"
#include "database/Session.hpp"
#include "database/User.hpp"
#include "services/auth/IPasswordService.hpp"

#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"
#include "Tooltip.hpp"
#include "common/DoubleValidator.hpp"
#include "common/MandatoryValidator.hpp"
#include "common/PasswordValidator.hpp"
#include "common/UUIDValidator.hpp"
#include "common/ValueStringModel.hpp"

namespace lms::ui
{
    using namespace db;

    class SettingsModel : public Wt::WFormModel
    {
    public:
        // Associate each field with a unique string literal.
        static inline const Field ArtistReleaseSortMethodField{ "artist-release-sort-method" };
        static inline const Field TranscodingModeField{ "transcoding-mode" };
        static inline const Field TranscodeFormatField{ "transcoding-output-format" };
        static inline const Field TranscodeBitrateField{ "transcoding-output-bitrate" };
        static inline const Field ReplayGainModeField{ "replaygain-mode" };
        static inline const Field ReplayGainPreAmpGainField{ "replaygain-preamp" };
        static inline const Field ReplayGainPreAmpGainIfNoInfoField{ "replaygain-preamp-no-rg-info" };
        static inline const Field SubsonicEnableTranscodingByDefault{ "subsonic-enable-transcoding-by-default" };
        static inline const Field SubsonicArtistListModeField{ "subsonic-artist-list-mode" };
        static inline const Field SubsonicTranscodingOutputFormatField{ "subsonic-transcoding-output-format" };
        static inline const Field SubsonicTranscodingOutputBitrateField{ "subsonic-transcoding-output-bitrate" };
        static inline const Field FeedbackBackendField{ "feedback-backend" };
        static inline const Field ScrobblingBackendField{ "scrobbling-backend" };
        static inline const Field ListenBrainzTokenField{ "listenbrainz-token" };
        static inline const Field PasswordOldField{ "password-old" };
        static inline const Field PasswordField{ "password" };
        static inline const Field PasswordConfirmField{ "password-confirm" };

        using ArtistReleaseSortMethodModel = ValueStringModel<db::ReleaseSortMethod>;
        using TranscodingModeModel = ValueStringModel<MediaPlayer::Settings::Transcoding::Mode>;
        using ReplayGainModeModel = ValueStringModel<MediaPlayer::Settings::ReplayGain::Mode>;
        using FeedbackBackendModel = ValueStringModel<db::FeedbackBackend>;
        using ScrobblingBackendModel = ValueStringModel<db::ScrobblingBackend>;

        SettingsModel(auth::IPasswordService* authPasswordService, bool withOldPassword)
            : _authPasswordService{ authPasswordService }
            , _withOldPassword{ withOldPassword }
        {
            initializeModels();

            addField(ArtistReleaseSortMethodField);
            addField(TranscodingModeField);
            addField(TranscodeBitrateField);
            addField(TranscodeFormatField);
            addField(ReplayGainModeField);
            addField(ReplayGainPreAmpGainField);
            addField(ReplayGainPreAmpGainIfNoInfoField);
            addField(SubsonicEnableTranscodingByDefault);
            addField(SubsonicTranscodingOutputBitrateField);
            addField(SubsonicTranscodingOutputFormatField);
            addField(FeedbackBackendField);
            addField(ScrobblingBackendField);
            addField(ListenBrainzTokenField);
            setValidator(ListenBrainzTokenField, createUUIDValidator());

            if (_authPasswordService)
            {
                if (_withOldPassword)
                {
                    addField(PasswordOldField);
                    setValidator(PasswordOldField, createPasswordCheckValidator());
                }

                addField(PasswordField);
                setValidator(PasswordField, createPasswordStrengthValidator([] { return auth::PasswordValidationContext{ .loginName = std::string{ LmsApp->getUserLoginName() }, .userType = LmsApp->getUserType() }; }));
                addField(PasswordConfirmField);
            }

            setValidator(ArtistReleaseSortMethodField, createMandatoryValidator());
            setValidator(TranscodingModeField, createMandatoryValidator());
            setValidator(TranscodeBitrateField, createMandatoryValidator());
            setValidator(TranscodeFormatField, createMandatoryValidator());
            setValidator(ReplayGainModeField, createMandatoryValidator());
            auto createPreAmpValidator{ [] {
                return createDoubleValidator(MediaPlayer::Settings::ReplayGain::minPreAmpGain, MediaPlayer::Settings::ReplayGain::maxPreAmpGain);
            } };

            setValidator(ReplayGainPreAmpGainField, createPreAmpValidator());
            setValidator(ReplayGainPreAmpGainIfNoInfoField, createPreAmpValidator());
            setValidator(SubsonicTranscodingOutputBitrateField, createMandatoryValidator());
            setValidator(SubsonicTranscodingOutputFormatField, createMandatoryValidator());

            loadData();
        }

        std::shared_ptr<ArtistReleaseSortMethodModel> getArtistReleaseSortMethodModel() { return _artistReleaseSortMethodModel; }
        std::shared_ptr<TranscodingModeModel> getTranscodingModeModel() { return _transcodingModeModeModel; }
        std::shared_ptr<Wt::WAbstractItemModel> getTranscodingOutputBitrateModel() { return _transcodingOutputBitrateModel; }
        std::shared_ptr<Wt::WAbstractItemModel> getTranscodingOutputFormatModel() { return _transcodingOutputFormatModel; }
        std::shared_ptr<ReplayGainModeModel> getReplayGainModeModel() { return _replayGainModeModel; }
        std::shared_ptr<Wt::WAbstractItemModel> getSubsonicArtistListModeModel() { return _subsonicArtistListModeModel; }
        std::shared_ptr<FeedbackBackendModel> getFeedbackBackendModel() { return _feedbackBackendModel; }
        std::shared_ptr<ScrobblingBackendModel> getScrobblingBackendModel() { return _scrobblingBackendModel; }

        void saveData()
        {
            auto transaction{ LmsApp->getDbSession().createWriteTransaction() };

            User::pointer user{ LmsApp->getUser() };

            {
                auto artistReleaseSortMethodRow{ _artistReleaseSortMethodModel->getRowFromString(valueText(ArtistReleaseSortMethodField)) };
                if (artistReleaseSortMethodRow)
                    user.modify()->setUIArtistReleaseSortMethod(_artistReleaseSortMethodModel->getValue(*artistReleaseSortMethodRow));
            }

            {
                MediaPlayer::Settings settings;

                auto transcodingModeRow{ _transcodingModeModeModel->getRowFromString(valueText(TranscodingModeField)) };
                if (transcodingModeRow)
                    settings.transcoding.mode = _transcodingModeModeModel->getValue(*transcodingModeRow);

                auto transcodingOutputFormatRow{ _transcodingOutputFormatModel->getRowFromString(valueText(TranscodeFormatField)) };
                if (transcodingOutputFormatRow)
                    settings.transcoding.format = _transcodingOutputFormatModel->getValue(*transcodingOutputFormatRow);

                auto transcodingOutputBitrateRow{ _transcodingOutputBitrateModel->getRowFromString(valueText(TranscodeBitrateField)) };
                if (transcodingOutputBitrateRow)
                    settings.transcoding.bitrate = _transcodingOutputBitrateModel->getValue(*transcodingOutputBitrateRow);

                auto replayGainModeRow{ _replayGainModeModel->getRowFromString(valueText(ReplayGainModeField)) };
                if (replayGainModeRow)
                    settings.replayGain.mode = _replayGainModeModel->getValue(*replayGainModeRow);

                settings.replayGain.preAmpGain = Wt::asNumber(value(ReplayGainPreAmpGainField));
                settings.replayGain.preAmpGainIfNoInfo = Wt::asNumber(value(ReplayGainPreAmpGainIfNoInfoField));

                LmsApp->getMediaPlayer().setSettings(settings);
            }

            {
                bool subsonicEnableTranscodingByDefault{ Wt::asNumber(value(SubsonicEnableTranscodingByDefault)) != 0 };
                user.modify()->setSubsonicEnableTranscodingByDefault(subsonicEnableTranscodingByDefault);

                auto subsonicTranscodingOutputBitrateRow{ _transcodingOutputBitrateModel->getRowFromString(valueText(SubsonicTranscodingOutputBitrateField)) };
                if (subsonicTranscodingOutputBitrateRow)
                    user.modify()->setSubsonicDefaultTranscodingOutputBitrate(_transcodingOutputBitrateModel->getValue(*subsonicTranscodingOutputBitrateRow));

                auto subsonicTranscodingOutputFormatRow{ _transcodingOutputFormatModel->getRowFromString(valueText(SubsonicTranscodingOutputFormatField)) };
                if (subsonicTranscodingOutputFormatRow)
                    user.modify()->setSubsonicDefaultTranscodintOutputFormat(_transcodingOutputFormatModel->getValue(*subsonicTranscodingOutputFormatRow));

                auto subsonicArtistListModeRow{ _subsonicArtistListModeModel->getRowFromString(valueText(SubsonicArtistListModeField)) };
                if (subsonicArtistListModeRow)
                    user.modify()->setSubsonicArtistListMode(_subsonicArtistListModeModel->getValue(*subsonicArtistListModeRow));
            }

            {
                if (auto feedbackBackendRow{ _feedbackBackendModel->getRowFromString(valueText(FeedbackBackendField)) })
                    user.modify()->setFeedbackBackend(_feedbackBackendModel->getValue(*feedbackBackendRow));

                user.modify()->setListenBrainzToken(core::UUID::fromString(Wt::asString(value(ListenBrainzTokenField)).toUTF8()));
            }

            {
                if (auto scrobblingBackendRow{ _scrobblingBackendModel->getRowFromString(valueText(ScrobblingBackendField)) })
                    user.modify()->setScrobblingBackend(_scrobblingBackendModel->getValue(*scrobblingBackendRow));

                user.modify()->setListenBrainzToken(core::UUID::fromString(Wt::asString(value(ListenBrainzTokenField)).toUTF8()));
            }

            if (_authPasswordService && !valueText(PasswordField).empty())
            {
                _authPasswordService->setPassword(user->getId(), valueText(PasswordField).toUTF8());
            }
        }

        void loadData()
        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            User::pointer user{ LmsApp->getUser() };

            {
                auto artistReleaseSortMethodRow{ _artistReleaseSortMethodModel->getRowFromValue(user->getUIArtistReleaseSortMethod()) };
                if (artistReleaseSortMethodRow)
                    setValue(ArtistReleaseSortMethodField, _artistReleaseSortMethodModel->getString(*artistReleaseSortMethodRow));
            }

            {
                const auto settings{ *LmsApp->getMediaPlayer().getSettings() };

                auto transcodingModeRow{ _transcodingModeModeModel->getRowFromValue(settings.transcoding.mode) };
                if (transcodingModeRow)
                    setValue(TranscodingModeField, _transcodingModeModeModel->getString(*transcodingModeRow));

                auto transcodingOutputFormatRow{ _transcodingOutputFormatModel->getRowFromValue(settings.transcoding.format) };
                if (transcodingOutputFormatRow)
                    setValue(TranscodeFormatField, _transcodingOutputFormatModel->getString(*transcodingOutputFormatRow));

                auto transcodingOutputBitrateRow{ _transcodingOutputBitrateModel->getRowFromValue(settings.transcoding.bitrate) };
                if (transcodingOutputBitrateRow)
                    setValue(TranscodeBitrateField, _transcodingOutputBitrateModel->getString(*transcodingOutputBitrateRow));

                {
                    const bool useTranscoding{ settings.transcoding.mode != MediaPlayer::Settings::Transcoding::Mode::Never };
                    setReadOnly(SettingsModel::TranscodeFormatField, !useTranscoding);
                    setReadOnly(SettingsModel::TranscodeBitrateField, !useTranscoding);
                }

                auto replayGainModeRow{ _replayGainModeModel->getRowFromValue(settings.replayGain.mode) };
                if (replayGainModeRow)
                    setValue(ReplayGainModeField, _replayGainModeModel->getString(*replayGainModeRow));

                setValue(ReplayGainPreAmpGainField, settings.replayGain.preAmpGain);
                setValue(ReplayGainPreAmpGainIfNoInfoField, settings.replayGain.preAmpGainIfNoInfo);
            }

            {
                setValue(SubsonicEnableTranscodingByDefault, user->getSubsonicEnableTranscodingByDefault());

                auto subsonicTranscodingOutputBitrateRow{ _transcodingOutputBitrateModel->getRowFromValue(user->getSubsonicDefaultTranscodingOutputBitrate()) };
                if (subsonicTranscodingOutputBitrateRow)
                    setValue(SubsonicTranscodingOutputBitrateField, _transcodingOutputBitrateModel->getString(*subsonicTranscodingOutputBitrateRow));

                auto subsonicTranscodingOutputFormatRow{ _transcodingOutputFormatModel->getRowFromValue(user->getSubsonicDefaultTranscodingOutputFormat()) };
                if (subsonicTranscodingOutputFormatRow)
                    setValue(SubsonicTranscodingOutputFormatField, _transcodingOutputFormatModel->getString(*subsonicTranscodingOutputFormatRow));

                auto subsonicArtistListModeRow{ _subsonicArtistListModeModel->getRowFromValue(user->getSubsonicArtistListMode()) };
                if (subsonicArtistListModeRow)
                    setValue(SubsonicArtistListModeField, _subsonicArtistListModeModel->getString(*subsonicArtistListModeRow));
            }

            {
                if (auto feedbackBackendRow{ _feedbackBackendModel->getRowFromValue(user->getFeedbackBackend()) })
                    setValue(FeedbackBackendField, _feedbackBackendModel->getString(*feedbackBackendRow));

                if (auto scrobblingBackendRow{ _scrobblingBackendModel->getRowFromValue(user->getScrobblingBackend()) })
                    setValue(ScrobblingBackendField, _scrobblingBackendModel->getString(*scrobblingBackendRow));

                if (auto listenBrainzToken{ user->getListenBrainzToken() })
                    setValue(ListenBrainzTokenField, Wt::WString::fromUTF8(std::string{ listenBrainzToken->getAsString() }));

                {
                    const bool usesListenBrainz{ user->getScrobblingBackend() == ScrobblingBackend::ListenBrainz || user->getFeedbackBackend() == FeedbackBackend::ListenBrainz };
                    setReadOnly(SettingsModel::ListenBrainzTokenField, !usesListenBrainz);
                    validator(SettingsModel::ListenBrainzTokenField)->setMandatory(usesListenBrainz);
                }
            }
            if (_authPasswordService)
            {
                if (_withOldPassword)
                    setValue(PasswordOldField, "");

                setValue(PasswordField, "");
                setValue(PasswordConfirmField, "");
            }
        }

    private:
        bool validateField(Field field)
        {
            Wt::WString error;

            if (field == PasswordOldField)
            {
                if (valueText(PasswordOldField).empty() && !valueText(PasswordField).empty())
                    error = Wt::WString::tr("Lms.Settings.password-must-fill-old-password");
                else
                    return Wt::WFormModel::validateField(field);
            }
            else if (field == PasswordField)
            {
                if (!valueText(PasswordOldField).empty() && valueText(PasswordField).empty())
                    error = Wt::WString::tr("Wt.WValidator.Invalid");
                else
                    return Wt::WFormModel::validateField(field);
            }
            else if (field == PasswordConfirmField)
            {
                if (validation(PasswordField).state() == Wt::ValidationState::Valid)
                {
                    if (valueText(PasswordField) != valueText(PasswordConfirmField))
                        error = Wt::WString::tr("Lms.passwords-dont-match");
                }
            }
            else
            {
                return Wt::WFormModel::validateField(field);
            }

            setValidation(field, Wt::WValidator::Result(error.empty() ? Wt::ValidationState::Valid : Wt::ValidationState::Invalid, error));

            return (validation(field).state() == Wt::ValidationState::Valid);
        }

    private:
        void initializeModels()
        {
            _artistReleaseSortMethodModel = std::make_shared<ArtistReleaseSortMethodModel>();
            _artistReleaseSortMethodModel->add(Wt::WString::tr("Lms.Settings.date-asc"), db::ReleaseSortMethod::DateAsc);
            _artistReleaseSortMethodModel->add(Wt::WString::tr("Lms.Settings.date-desc"), db::ReleaseSortMethod::DateDesc);
            _artistReleaseSortMethodModel->add(Wt::WString::tr("Lms.Settings.original-date-asc"), db::ReleaseSortMethod::OriginalDate);
            _artistReleaseSortMethodModel->add(Wt::WString::tr("Lms.Settings.original-date-desc"), db::ReleaseSortMethod::OriginalDateDesc);
            _artistReleaseSortMethodModel->add(Wt::WString::tr("Lms.Settings.name"), db::ReleaseSortMethod::Name);

            _transcodingModeModeModel = std::make_shared<TranscodingModeModel>();
            _transcodingModeModeModel->add(Wt::WString::tr("Lms.Settings.transcoding-mode.always"), MediaPlayer::Settings::Transcoding::Mode::Always);
            _transcodingModeModeModel->add(Wt::WString::tr("Lms.Settings.transcoding-mode.never"), MediaPlayer::Settings::Transcoding::Mode::Never);
            _transcodingModeModeModel->add(Wt::WString::tr("Lms.Settings.transcoding-mode.if-format-not-supported"), MediaPlayer::Settings::Transcoding::Mode::IfFormatNotSupported);

            _transcodingOutputBitrateModel = std::make_shared<ValueStringModel<Bitrate>>();
            visitAllowedAudioBitrates([&](const Bitrate bitrate) {
                _transcodingOutputBitrateModel->add(Wt::WString::fromUTF8(std::to_string(bitrate / 1000)), bitrate);
            });

            _transcodingOutputFormatModel = std::make_shared<ValueStringModel<TranscodingOutputFormat>>();
            _transcodingOutputFormatModel->add(Wt::WString::tr("Lms.Settings.transcoding-output-format.mp3"), TranscodingOutputFormat::MP3);
            _transcodingOutputFormatModel->add(Wt::WString::tr("Lms.Settings.transcoding-output-format.ogg_opus"), TranscodingOutputFormat::OGG_OPUS);
            _transcodingOutputFormatModel->add(Wt::WString::tr("Lms.Settings.transcoding-output-format.matroska_opus"), TranscodingOutputFormat::MATROSKA_OPUS);
            _transcodingOutputFormatModel->add(Wt::WString::tr("Lms.Settings.transcoding-output-format.ogg_vorbis"), TranscodingOutputFormat::OGG_VORBIS);
            _transcodingOutputFormatModel->add(Wt::WString::tr("Lms.Settings.transcoding-output-format.webm_vorbis"), TranscodingOutputFormat::WEBM_VORBIS);

            _replayGainModeModel = std::make_shared<ReplayGainModeModel>();
            _replayGainModeModel->add(Wt::WString::tr("Lms.Settings.replaygain-mode.none"), MediaPlayer::Settings::ReplayGain::Mode::None);
            _replayGainModeModel->add(Wt::WString::tr("Lms.Settings.replaygain-mode.auto"), MediaPlayer::Settings::ReplayGain::Mode::Auto);
            _replayGainModeModel->add(Wt::WString::tr("Lms.Settings.replaygain-mode.track"), MediaPlayer::Settings::ReplayGain::Mode::Track);
            _replayGainModeModel->add(Wt::WString::tr("Lms.Settings.replaygain-mode.release"), MediaPlayer::Settings::ReplayGain::Mode::Release);

            _subsonicArtistListModeModel = std::make_shared<ValueStringModel<SubsonicArtistListMode>>();
            _subsonicArtistListModeModel->add(Wt::WString::tr("Lms.Settings.subsonic-artist-list-mode.all-artists"), SubsonicArtistListMode::AllArtists);
            _subsonicArtistListModeModel->add(Wt::WString::tr("Lms.Settings.subsonic-artist-list-mode.release-artists"), SubsonicArtistListMode::ReleaseArtists);
            _subsonicArtistListModeModel->add(Wt::WString::tr("Lms.Settings.subsonic-artist-list-mode.track-artists"), SubsonicArtistListMode::TrackArtists);

            _feedbackBackendModel = std::make_shared<ValueStringModel<FeedbackBackend>>();
            _feedbackBackendModel->add(Wt::WString::tr("Lms.Settings.backend.internal"), FeedbackBackend::Internal);
            _feedbackBackendModel->add(Wt::WString::tr("Lms.Settings.backend.listenbrainz"), FeedbackBackend::ListenBrainz);

            _scrobblingBackendModel = std::make_shared<ValueStringModel<ScrobblingBackend>>();
            _scrobblingBackendModel->add(Wt::WString::tr("Lms.Settings.backend.internal"), ScrobblingBackend::Internal);
            _scrobblingBackendModel->add(Wt::WString::tr("Lms.Settings.backend.listenbrainz"), ScrobblingBackend::ListenBrainz);
        }

        auth::IPasswordService* _authPasswordService{};
        bool _withOldPassword{};

        std::shared_ptr<ArtistReleaseSortMethodModel> _artistReleaseSortMethodModel;
        std::shared_ptr<TranscodingModeModel> _transcodingModeModeModel;
        std::shared_ptr<ValueStringModel<Bitrate>> _transcodingOutputBitrateModel;
        std::shared_ptr<ValueStringModel<TranscodingOutputFormat>> _transcodingOutputFormatModel;
        std::shared_ptr<ReplayGainModeModel> _replayGainModeModel;
        std::shared_ptr<ValueStringModel<SubsonicArtistListMode>> _subsonicArtistListModeModel;
        std::shared_ptr<FeedbackBackendModel> _feedbackBackendModel;
        std::shared_ptr<ScrobblingBackendModel> _scrobblingBackendModel;
    };

    SettingsView::SettingsView()
    {
        wApp->internalPathChanged().connect(this, [this] {
            refreshView();
        });

        LmsApp->getMediaPlayer().settingsLoaded.connect([this] {
            refreshView();
        });

        refreshView();
    }

    void
    SettingsView::refreshView()
    {
        if (!wApp->internalPathMatches("/settings"))
            return;

        clear();

        // Hack to wait for the audio player know the settings applied
        if (!LmsApp->getMediaPlayer().getSettings())
            return;

        auto t{ addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Settings.template")) };

        auto* authPasswordService{ core::Service<auth::IPasswordService>::get() };
        if (authPasswordService && !authPasswordService->canSetPasswords())
            authPasswordService = nullptr;

        auto model{ std::make_shared<SettingsModel>(authPasswordService, !LmsApp->isUserAuthStrong()) };

        if (authPasswordService)
        {
            t->setCondition("if-has-change-password", true);

            // Old password
            if (!LmsApp->isUserAuthStrong())
            {
                t->setCondition("if-has-old-password", true);

                auto oldPassword{ std::make_unique<Wt::WLineEdit>() };
                oldPassword->setEchoMode(Wt::EchoMode::Password);
                oldPassword->setAttributeValue("autocomplete", "current-password");
                t->setFormWidget(SettingsModel::PasswordOldField, std::move(oldPassword));
            }

            // Password
            auto password{ std::make_unique<Wt::WLineEdit>() };
            password->setEchoMode(Wt::EchoMode::Password);
            password->setAttributeValue("autocomplete", "new-password");
            t->setFormWidget(SettingsModel::PasswordField, std::move(password));

            // Password confirm
            auto passwordConfirm{ std::make_unique<Wt::WLineEdit>() };
            passwordConfirm->setEchoMode(Wt::EchoMode::Password);
            passwordConfirm->setAttributeValue("autocomplete", "new-password");
            t->setFormWidget(SettingsModel::PasswordConfirmField, std::move(passwordConfirm));
        }

        // User interface
        {
            auto artistReleaseSortMethod{ std::make_unique<Wt::WComboBox>() };
            artistReleaseSortMethod->setModel(model->getArtistReleaseSortMethodModel());
            t->setFormWidget(SettingsModel::ArtistReleaseSortMethodField, std::move(artistReleaseSortMethod));
        }

        // Audio
        {
            // Transcode
            auto transcodingMode{ std::make_unique<Wt::WComboBox>() };
            transcodingMode->setModel(model->getTranscodingModeModel());
            transcodingMode->activated().connect([=](int row) {
                const bool enable{ model->getTranscodingModeModel()->getValue(row) != MediaPlayer::Settings::Transcoding::Mode::Never };
                model->setReadOnly(SettingsModel::TranscodeFormatField, !enable);
                model->setReadOnly(SettingsModel::TranscodeBitrateField, !enable);
                t->updateModel(model.get());
                t->updateView(model.get());
            });
            t->setFormWidget(SettingsModel::TranscodingModeField, std::move(transcodingMode));

            // Format
            auto transcodingOutputFormat{ std::make_unique<Wt::WComboBox>() };
            transcodingOutputFormat->setModel(model->getTranscodingOutputFormatModel());
            t->setFormWidget(SettingsModel::TranscodeFormatField, std::move(transcodingOutputFormat));

            // Bitrate
            auto transcodingOutputBitrate{ std::make_unique<Wt::WComboBox>() };
            transcodingOutputBitrate->setModel(model->getTranscodingOutputBitrateModel());
            t->setFormWidget(SettingsModel::TranscodeBitrateField, std::move(transcodingOutputBitrate));

            // Replay gain mode
            auto replayGainMode{ std::make_unique<Wt::WComboBox>() };
            replayGainMode->activated().connect([=](int row) {
                const bool enable{ model->getReplayGainModeModel()->getValue(row) != MediaPlayer::Settings::ReplayGain::Mode::None };
                model->setReadOnly(SettingsModel::SettingsModel::ReplayGainPreAmpGainField, !enable);
                model->setReadOnly(SettingsModel::SettingsModel::ReplayGainPreAmpGainIfNoInfoField, !enable);
                t->updateModel(model.get());
                t->updateView(model.get());
            });
            replayGainMode->setModel(model->getReplayGainModeModel());
            t->setFormWidget(SettingsModel::ReplayGainModeField, std::move(replayGainMode));

            // Replay gain preampGain
            auto replayGainPreampGain{ std::make_unique<Wt::WDoubleSpinBox>() };
            replayGainPreampGain->setRange(MediaPlayer::Settings::ReplayGain::minPreAmpGain, MediaPlayer::Settings::ReplayGain::maxPreAmpGain);
            t->setFormWidget(SettingsModel::ReplayGainPreAmpGainField, std::move(replayGainPreampGain));

            // Replay gain preampGain if no info
            auto replayGainPreampGainIfNoInfo{ std::make_unique<Wt::WDoubleSpinBox>() };
            replayGainPreampGainIfNoInfo->setRange(MediaPlayer::Settings::ReplayGain::minPreAmpGain, MediaPlayer::Settings::ReplayGain::maxPreAmpGain);
            t->setFormWidget(SettingsModel::ReplayGainPreAmpGainIfNoInfoField, std::move(replayGainPreampGainIfNoInfo));

            if (LmsApp->getMediaPlayer().getSettings()->replayGain.mode == MediaPlayer::Settings::ReplayGain::Mode::None)
            {
                model->setReadOnly(SettingsModel::SettingsModel::ReplayGainPreAmpGainField, true);
                model->setReadOnly(SettingsModel::SettingsModel::ReplayGainPreAmpGainIfNoInfoField, true);
            }
        }

        // Subsonic
        {
            t->setCondition("if-has-subsonic-api", core::Service<core::IConfig>::get()->getBool("api-subsonic", true));

            // Enable transcoding by default
            t->setFormWidget(SettingsModel::SubsonicEnableTranscodingByDefault, std::make_unique<Wt::WCheckBox>());

            // Format
            auto transcodingOutputFormat{ std::make_unique<Wt::WComboBox>() };
            transcodingOutputFormat->setModel(model->getTranscodingOutputFormatModel());
            t->setFormWidget(SettingsModel::SubsonicTranscodingOutputFormatField, std::move(transcodingOutputFormat));

            // Bitrate
            auto transcodingOutputBitrate{ std::make_unique<Wt::WComboBox>() };
            transcodingOutputBitrate->setModel(model->getTranscodingOutputBitrateModel());
            t->setFormWidget(SettingsModel::SubsonicTranscodingOutputBitrateField, std::move(transcodingOutputBitrate));

            // Artist list mode
            auto artistListMode{ std::make_unique<Wt::WComboBox>() };
            artistListMode->setModel(model->getSubsonicArtistListModeModel());
            t->setFormWidget(SettingsModel::SubsonicArtistListModeField, std::move(artistListMode));
        }

        // Feedback
        Wt::WComboBox* feedbackBackendRaw{};
        {
            auto feedbackBackend{ std::make_unique<Wt::WComboBox>() };
            feedbackBackend->setModel(model->getFeedbackBackendModel());
            feedbackBackendRaw = feedbackBackend.get();
            t->setFormWidget(SettingsModel::FeedbackBackendField, std::move(feedbackBackend));
        }

        // Scrobbling
        Wt::WComboBox* scrobblingBackendRaw;
        {
            auto scrobblingBackend{ std::make_unique<Wt::WComboBox>() };
            scrobblingBackend->setModel(model->getScrobblingBackendModel());
            scrobblingBackendRaw = scrobblingBackend.get();
            t->setFormWidget(SettingsModel::ScrobblingBackendField, std::move(scrobblingBackend));
        }

        // Backend settings
        {
            auto listenbrainzToken{ std::make_unique<Wt::WLineEdit>() };
            t->setFormWidget(SettingsModel::ListenBrainzTokenField, std::move(listenbrainzToken));
        }

        auto updateListenBrainzTokenField{ [=] {
            const bool enable{ model->getFeedbackBackendModel()->getValue(feedbackBackendRaw->currentIndex()) == FeedbackBackend::ListenBrainz
                               || model->getScrobblingBackendModel()->getValue(scrobblingBackendRaw->currentIndex()) == ScrobblingBackend::ListenBrainz };

            model->setReadOnly(SettingsModel::ListenBrainzTokenField, !enable);
            model->validator(SettingsModel::ListenBrainzTokenField)->setMandatory(enable);
            t->updateModel(model.get());
            t->updateView(model.get());
        } };

        feedbackBackendRaw->activated().connect([=] { updateListenBrainzTokenField(); });
        scrobblingBackendRaw->activated().connect([=] { updateListenBrainzTokenField(); });

        // Buttons
        Wt::WPushButton* saveBtn{ t->bindWidget("save-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.save"))) };
        Wt::WPushButton* discardBtn{ t->bindWidget("discard-btn", std::make_unique<Wt::WPushButton>(Wt::WString::tr("Lms.discard"))) };

        saveBtn->clicked().connect([=] {
            {
                if (LmsApp->getUserType() == db::UserType::DEMO)
                {
                    LmsApp->notifyMsg(Notification::Type::Warning, Wt::WString::tr("Lms.Settings.settings"), Wt::WString::tr("Lms.Settings.demo-cannot-save"));
                    return;
                }
            }

            t->updateModel(model.get());

            if (model->validate())
            {
                model->saveData();
                LmsApp->notifyMsg(Notification::Type::Info, Wt::WString::tr("Lms.Settings.settings"), Wt::WString::tr("Lms.Settings.settings-saved"));
            }

            // Udate the view: Delete any validation message in the view, etc.
            t->updateView(model.get());
        });

        discardBtn->clicked().connect([=] {
            model->loadData();
            model->validate();
            t->updateView(model.get());
        });

        t->updateView(model.get());

        initTooltipsForWidgetTree(*t);
    }

} // namespace lms::ui