/*
 * Copyright (C) 2024 Emeric Poupon
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

#pragma once

#include <memory>
#include <vector>

#include "FileToScan.hpp"
#include "IFileScanOperation.hpp"

namespace lms::db
{
    class IDb;
} // namespace lms::db

namespace lms::scanner
{
    struct ScanError;
    struct ScannerSettings;

    class FileScanOperationBase : public IFileScanOperation
    {
    public:
        FileScanOperationBase(FileToScan&& fileToScan, db::IDb& db, const ScannerSettings& settings);
        ~FileScanOperationBase() override;
        FileScanOperationBase(const FileScanOperationBase&) = delete;
        FileScanOperationBase& operator=(const FileScanOperationBase&) = delete;

    protected:
        const std::filesystem::path& getFilePath() const override { return _file.filePath; }
        const MediaLibraryInfo& getMediaLibrary() const { return _file.mediaLibrary; }
        db::IDb& getDb() { return _db; }
        const ScannerSettings& getScannerSettings() const { return _settings; }
        Wt::WDateTime getLastWriteTime() const { return _file.lastWriteTime; }
        std::size_t getFileSize() const { return _file.fileSize; }

        template<typename T, typename... CtrArgs>
        void addError(CtrArgs&&... args)
        {
            _errors.emplace_back(std::make_shared<T>(std::forward<CtrArgs>(args)...));
        }

        const ScanErrorVector& getErrors() override { return _errors; }

    private:
        const FileToScan _file;
        db::IDb& _db;
        const ScannerSettings& _settings;
        ScanErrorVector _errors;
    };
} // namespace lms::scanner