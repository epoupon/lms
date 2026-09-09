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

#include "AudioSettingsView.hpp"

#include <Wt/WComboBox.h>
#include <Wt/WDoubleSpinBox.h>
#include <Wt/WFormModel.h>
#include <Wt/WString.h>
#include <Wt/WTemplateFormView.h>

#include "database/Types.hpp"

#include "LmsApplication.hpp"
#include "MediaPlayer.hpp"
#include "Tooltip.hpp"
#include "common/DoubleValidator.hpp"
#include "common/MandatoryValidator.hpp"
#include "common/TranscodingOutputFormatModel.hpp"
#include "common/ValueStringModel.hpp"

#include "SettingsViewUtils.hpp"

namespace lms::ui
{
    namespace
    {
        class AudioSettingsModel : public Wt::WFormModel
        {
        public:
            static inline const Field TranscodingModeField{ "transcoding-mode" };
            static inline const Field TranscodeFormatField{ "transcoding-output-format" };
            static inline const Field TranscodeBitrateField{ "transcoding-output-bitrate" };
            static inline const Field ReplayGainModeField{ "replaygain-mode" };
            static inline const Field ReplayGainPreAmpGainField{ "replaygain-preamp" };
            static inline const Field ReplayGainPreAmpGainIfNoInfoField{ "replaygain-preamp-no-rg-info" };

            using TranscodingModeModel = ValueStringModel<MediaPlayer::Settings::Transcoding::Mode>;
            using ReplayGainModeModel = ValueStringModel<MediaPlayer::Settings::ReplayGain::Mode>;

            AudioSettingsModel()
            {
                _transcodingModeModeModel = std::make_shared<TranscodingModeModel>();
                _transcodingModeModeModel->add(Wt::WString::tr("Lms.Settings.transcoding-mode.always"), MediaPlayer::Settings::Transcoding::Mode::Always);
                _transcodingModeModeModel->add(Wt::WString::tr("Lms.Settings.transcoding-mode.never"), MediaPlayer::Settings::Transcoding::Mode::Never);
                _transcodingModeModeModel->add(Wt::WString::tr("Lms.Settings.transcoding-mode.if-format-not-supported"), MediaPlayer::Settings::Transcoding::Mode::IfFormatNotSupported);

                _transcodingOutputBitrateModel = std::make_shared<ValueStringModel<db::Bitrate>>();
                db::visitAllowedAudioBitrates([&](const db::Bitrate bitrate) {
                    _transcodingOutputBitrateModel->add(Wt::WString::fromUTF8(std::to_string(bitrate / 1000)), bitrate);
                });

                _transcodingOutputFormatModel = createTranscodingOutputFormatModel();

                _replayGainModeModel = std::make_shared<ReplayGainModeModel>();
                _replayGainModeModel->add(Wt::WString::tr("Lms.Settings.replaygain-mode.none"), MediaPlayer::Settings::ReplayGain::Mode::None);
                _replayGainModeModel->add(Wt::WString::tr("Lms.Settings.replaygain-mode.auto"), MediaPlayer::Settings::ReplayGain::Mode::Auto);
                _replayGainModeModel->add(Wt::WString::tr("Lms.Settings.replaygain-mode.track"), MediaPlayer::Settings::ReplayGain::Mode::Track);
                _replayGainModeModel->add(Wt::WString::tr("Lms.Settings.replaygain-mode.release"), MediaPlayer::Settings::ReplayGain::Mode::Release);

                addField(TranscodingModeField);
                addField(TranscodeBitrateField);
                addField(TranscodeFormatField);
                addField(ReplayGainModeField);
                addField(ReplayGainPreAmpGainField);
                addField(ReplayGainPreAmpGainIfNoInfoField);

                setValidator(TranscodingModeField, createMandatoryValidator());
                setValidator(TranscodeBitrateField, createMandatoryValidator());
                setValidator(TranscodeFormatField, createTranscodingOutputFormatValidator(_transcodingOutputFormatModel));
                setValidator(ReplayGainModeField, createMandatoryValidator());

                auto createPreAmpValidator{ [] {
                    return createDoubleValidator(MediaPlayer::Settings::ReplayGain::minPreAmpGain, MediaPlayer::Settings::ReplayGain::maxPreAmpGain);
                } };
                setValidator(ReplayGainPreAmpGainField, createPreAmpValidator());
                setValidator(ReplayGainPreAmpGainIfNoInfoField, createPreAmpValidator());

                loadData();
            }

            std::shared_ptr<TranscodingModeModel> getTranscodingModeModel() { return _transcodingModeModeModel; }
            std::shared_ptr<Wt::WAbstractItemModel> getTranscodingOutputBitrateModel() { return _transcodingOutputBitrateModel; }
            std::shared_ptr<Wt::WAbstractItemModel> getTranscodingOutputFormatModel() { return _transcodingOutputFormatModel; }
            std::shared_ptr<ReplayGainModeModel> getReplayGainModeModel() { return _replayGainModeModel; }

            void saveData()
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

            void loadData()
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
                    setReadOnly(AudioSettingsModel::TranscodeFormatField, !useTranscoding);
                    setReadOnly(AudioSettingsModel::TranscodeBitrateField, !useTranscoding);
                }

                auto replayGainModeRow{ _replayGainModeModel->getRowFromValue(settings.replayGain.mode) };
                if (replayGainModeRow)
                    setValue(ReplayGainModeField, _replayGainModeModel->getString(*replayGainModeRow));

                setValue(ReplayGainPreAmpGainField, settings.replayGain.preAmpGain);
                setValue(ReplayGainPreAmpGainIfNoInfoField, settings.replayGain.preAmpGainIfNoInfo);

                if (settings.replayGain.mode == MediaPlayer::Settings::ReplayGain::Mode::None)
                {
                    setReadOnly(AudioSettingsModel::ReplayGainPreAmpGainField, true);
                    setReadOnly(AudioSettingsModel::ReplayGainPreAmpGainIfNoInfoField, true);
                }
            }

        private:
            std::shared_ptr<TranscodingModeModel> _transcodingModeModeModel;
            std::shared_ptr<ValueStringModel<db::Bitrate>> _transcodingOutputBitrateModel;
            std::shared_ptr<ValueStringModel<db::TranscodingOutputFormat>> _transcodingOutputFormatModel;
            std::shared_ptr<ReplayGainModeModel> _replayGainModeModel;
        };
    } // namespace

    AudioSettingsView::AudioSettingsView()
    {
        wApp->internalPathChanged().connect(this, [this] {
            refreshView();
        });

        LmsApp->getMediaPlayer().settingsLoaded.connect([this] {
            refreshView();
        });

        refreshView();
    }

    void AudioSettingsView::refreshView()
    {
        if (!wApp->internalPathMatches("/settings/audio"))
            return;

        clear();

        if (!LmsApp->getMediaPlayer().getSettings())
            return;

        auto* t{ addNew<Wt::WTemplateFormView>(Wt::WString::tr("Lms.Settings.audio.template")) };
        auto model{ std::make_shared<AudioSettingsModel>() };

        auto transcodingMode{ std::make_unique<Wt::WComboBox>() };
        transcodingMode->setModel(model->getTranscodingModeModel());
        transcodingMode->activated().connect([=](int row) {
            const bool enable{ model->getTranscodingModeModel()->getValue(row) != MediaPlayer::Settings::Transcoding::Mode::Never };
            model->setReadOnly(AudioSettingsModel::TranscodeFormatField, !enable);
            model->setReadOnly(AudioSettingsModel::TranscodeBitrateField, !enable);
            t->updateModel(model.get());
            t->updateView(model.get());
        });
        t->setFormWidget(AudioSettingsModel::TranscodingModeField, std::move(transcodingMode));

        auto transcodingOutputFormat{ std::make_unique<Wt::WComboBox>() };
        transcodingOutputFormat->setModel(model->getTranscodingOutputFormatModel());
        t->setFormWidget(AudioSettingsModel::TranscodeFormatField, std::move(transcodingOutputFormat));

        auto transcodingOutputBitrate{ std::make_unique<Wt::WComboBox>() };
        transcodingOutputBitrate->setModel(model->getTranscodingOutputBitrateModel());
        t->setFormWidget(AudioSettingsModel::TranscodeBitrateField, std::move(transcodingOutputBitrate));

        auto replayGainMode{ std::make_unique<Wt::WComboBox>() };
        replayGainMode->activated().connect([=](int row) {
            const bool enable{ model->getReplayGainModeModel()->getValue(row) != MediaPlayer::Settings::ReplayGain::Mode::None };
            model->setReadOnly(AudioSettingsModel::ReplayGainPreAmpGainField, !enable);
            model->setReadOnly(AudioSettingsModel::ReplayGainPreAmpGainIfNoInfoField, !enable);
            t->updateModel(model.get());
            t->updateView(model.get());
        });
        replayGainMode->setModel(model->getReplayGainModeModel());
        t->setFormWidget(AudioSettingsModel::ReplayGainModeField, std::move(replayGainMode));

        auto replayGainPreampGain{ std::make_unique<Wt::WDoubleSpinBox>() };
        replayGainPreampGain->setRange(MediaPlayer::Settings::ReplayGain::minPreAmpGain, MediaPlayer::Settings::ReplayGain::maxPreAmpGain);
        t->setFormWidget(AudioSettingsModel::ReplayGainPreAmpGainField, std::move(replayGainPreampGain));

        auto replayGainPreampGainIfNoInfo{ std::make_unique<Wt::WDoubleSpinBox>() };
        replayGainPreampGainIfNoInfo->setRange(MediaPlayer::Settings::ReplayGain::minPreAmpGain, MediaPlayer::Settings::ReplayGain::maxPreAmpGain);
        t->setFormWidget(AudioSettingsModel::ReplayGainPreAmpGainIfNoInfoField, std::move(replayGainPreampGainIfNoInfo));

        utils::bindSaveDiscardButtons(t, model.get(), [model] { model->saveData(); }, [model] { model->loadData(); });
        t->updateView(model.get());
        initTooltipsForWidgetTree(*t);
    }
} // namespace lms::ui
