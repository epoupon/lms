/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "ScannerReportResource.hpp"

#include <Wt/Http/Response.h>
#include <Wt/Utils.h>

#include "core/String.hpp"
#include "database/Session.hpp"
#include "database/objects/Track.hpp"

#include "LmsApplication.hpp"
#include "services/scanner/ScanErrors.hpp"

namespace lms::ui
{
    namespace
    {
        class ErrorFormatter : public scanner::ScanErrorVisitor
        {
        public:
            ErrorFormatter(std::ostream& os)
                : _os{ os } {}

        private:
            void visit(const scanner::ScanError&) override
            {
            }

            void visit(const scanner::IOScanError& error) override
            {
                _os << error.path << ": " << Wt::WString::tr("Lms.Admin.ScannerController.cannot-read-file").arg(Wt::WString::fromUTF8(error.err.message())).toUTF8() << '\n';
            }

            void visit(const scanner::AudioFileScanError& error) override
            {
                _os << error.path << ": " << Wt::WString::tr("Lms.Admin.ScannerController.cannot-read-audio-file").toUTF8() << '\n';
            }
            void visit(const scanner::EmbeddedImageScanError& error) override
            {
                _os << error.path << ": " << Wt::WString::tr("Lms.Admin.ScannerController.bad-embedded-image").arg(error.index).arg(Wt::WString::fromUTF8(error.errorMsg)).toUTF8() << '\n';
            }
            void visit(const scanner::NoAudioTrackFoundError& error) override
            {
                _os << error.path << ": " << Wt::WString::tr("Lms.Admin.ScannerController.no-audio-track").toUTF8() << '\n';
            }
            void visit(const scanner::BadAudioDurationError& error) override
            {
                _os << error.path << ": " << Wt::WString::tr("Lms.Admin.ScannerController.bad-duration").toUTF8() << '\n';
            }
            void visit(const scanner::ArtistInfoFileScanError& error) override
            {
                _os << error.path << ": " << Wt::WString::tr("Lms.Admin.ScannerController.cannot-read-artist-info-file").toUTF8() << '\n';
            }
            void visit(const scanner::MissingArtistNameError& error) override
            {
                _os << error.path << ": " << Wt::WString::tr("Lms.Admin.ScannerController.missing-artist-name").toUTF8() << '\n';
            }
            void visit(const scanner::ImageFileScanError& error) override
            {
                _os << error.path << ": " << Wt::WString::tr("Lms.Admin.ScannerController.cannot-read-image-file").arg(Wt::WString::fromUTF8(error.errorMsg)).toUTF8() << '\n';
            }
            void visit(const scanner::LyricsFileScanError& error) override
            {
                _os << error.path << ": " << Wt::WString::tr("Lms.Admin.ScannerController.cannot-read-lyrics-file").toUTF8() << '\n';
            }
            void visit(const scanner::PlayListFileScanError& error) override
            {
                _os << error.path << ": " << Wt::WString::tr("Lms.Admin.ScannerController.cannot-read-playlist-file").toUTF8() << '\n';
            }
            void visit(const scanner::PlayListFilePathMissingError& error) override
            {
                _os << error.path << ": " << Wt::WString::tr("Lms.Admin.ScannerController.playlist-path-missing").arg(Wt::WString::fromUTF8(error.entry)).toUTF8() << '\n';
            }
            void visit(const scanner::PlayListFileAllPathesMissingError& error) override
            {
                _os << error.path << ": " << Wt::WString::tr("Lms.Admin.ScannerController.playlist-all-pathes-missing").toUTF8() << '\n';
            }

            std::ostream& _os;
        };
    } // namespace

    ScannerReportResource::ScannerReportResource() = default;
    ScannerReportResource::~ScannerReportResource()
    {
        beingDeleted();
    }

    void ScannerReportResource::setScanStats(const scanner::ScanStats& stats)
    {
        if (!_stats)
            _stats = std::make_unique<scanner::ScanStats>();

        *_stats = stats;
    }

    void ScannerReportResource::handleRequest(const Wt::Http::Request&, Wt::Http::Response& response)
    {
        if (!_stats)
            return;

        auto encodeHttpHeaderField = [](const std::string& fieldName, const std::string& fieldValue) {
            // This implements RFC 5987
            return fieldName + "*=UTF-8''" + Wt::Utils::urlEncode(fieldValue);
        };

        const std::string cdp{ encodeHttpHeaderField("filename", "LMS_scan_report_" + core::stringUtils::toISO8601String(_stats->startTime) + ".txt") };
        response.addHeader("Content-Disposition", "attachment; " + cdp);

        response.out() << Wt::WString::tr("Lms.Admin.ScannerController.errors-header").arg(_stats->errorsCount).toUTF8() << std::endl;

        ErrorFormatter errorFormatter{ response.out() };
        for (const auto& error : _stats->errors)
            error->accept(errorFormatter);

        response.out() << std::endl;

        response.out() << Wt::WString::tr("Lms.Admin.ScannerController.duplicates-header").arg(_stats->duplicates.size()).toUTF8() << std::endl;

        {
            auto transaction{ LmsApp->getDbSession().createReadTransaction() };

            for (const auto& duplicate : _stats->duplicates)
            {
                const auto& track{ db::Track::find(LmsApp->getDbSession(), duplicate.trackId) };
                if (!track)
                    continue;

                response.out() << track->getAbsoluteFilePath().string();
                if (auto mbid{ track->getTrackMBID() })
                    response.out() << " (Track MBID " << mbid->getAsString() << ")";

                response.out() << " - " << duplicateReasonToWString(duplicate.reason).toUTF8() << '\n';
            }
        }
    }

    Wt::WString ScannerReportResource::duplicateReasonToWString(scanner::DuplicateReason reason)
    {
        switch (reason)
        {
        case scanner::DuplicateReason::SameHash:
            return Wt::WString::tr("Lms.Admin.ScannerController.same-hash");
        case scanner::DuplicateReason::SameTrackMBID:
            return Wt::WString::tr("Lms.Admin.ScannerController.same-mbid");
        }
        return "?";
    }

} // namespace lms::ui
