/*
 * Copyright (C) 2026 Emeric Poupon
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

#include "TranscodingOutputFormatModel.hpp"

#include <array>

#include "Utils.hpp"

namespace lms::ui
{
    namespace
    {
        struct TranscodingOutputFormatEntry
        {
            db::TranscodingOutputFormat format;
            const char* trKey;
        };

        class TranscodingOutputFormatValidator : public Wt::WValidator
        {
        public:
            TranscodingOutputFormatValidator(std::shared_ptr<ValueStringModel<db::TranscodingOutputFormat>> model)
                : _model{ std::move(model) }
            {
            }

        private:
            Wt::WValidator::Result validate(const Wt::WString& input) const override
            {
                if (input.empty())
                    return Wt::WValidator::validate(input);

                const auto row{ _model->getRowFromString(input) };
                if (!row || !utils::toSupportedTranscodeOutputFormat(_model->getValue(*row)))
                    return Wt::WValidator::Result{ Wt::ValidationState::Invalid, Wt::WString::tr("Lms.Settings.transcoding-output-format-not-supported") };

                return Wt::WValidator::Result{ Wt::ValidationState::Valid };
            }

            std::string javaScriptValidate() const override { return {}; }

            std::shared_ptr<ValueStringModel<db::TranscodingOutputFormat>> _model;
        };
    } // namespace

    std::shared_ptr<ValueStringModel<db::TranscodingOutputFormat>> createTranscodingOutputFormatModel()
    {
        constexpr std::array transcodingOutputFormatEntries{
            TranscodingOutputFormatEntry{ db::TranscodingOutputFormat::MP3, "Lms.Settings.transcoding-output-format.mp3" },
            TranscodingOutputFormatEntry{ db::TranscodingOutputFormat::OGG_OPUS, "Lms.Settings.transcoding-output-format.ogg_opus" },
            TranscodingOutputFormatEntry{ db::TranscodingOutputFormat::OGG_VORBIS, "Lms.Settings.transcoding-output-format.ogg_vorbis" },
        };

        auto model{ std::make_shared<ValueStringModel<db::TranscodingOutputFormat>>() };
        for (const TranscodingOutputFormatEntry& entry : transcodingOutputFormatEntries)
            model->add(Wt::WString::tr(entry.trKey), entry.format);

        return model;
    }

    std::unique_ptr<Wt::WValidator> createTranscodingOutputFormatValidator(std::shared_ptr<ValueStringModel<db::TranscodingOutputFormat>> model)
    {
        auto validator{ std::make_unique<TranscodingOutputFormatValidator>(std::move(model)) };
        validator->setMandatory(true);

        return validator;
    }
} // namespace lms::ui
