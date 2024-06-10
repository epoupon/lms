/*
 * Copyright (C) 2021 Emeric Poupon
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

#include <optional>
#include <thread>

#include <boost/asio/io_service.hpp>

namespace lms::core
{
    class IOContextRunner
    {
    public:
        IOContextRunner(boost::asio::io_service& ioService, std::size_t threadCount, std::string_view name);
        ~IOContextRunner();

        void stop();
        std::size_t getThreadCount() const;

    private:
        IOContextRunner(const IOContextRunner&) = delete;
        IOContextRunner& operator=(const IOContextRunner&) = delete;

        boost::asio::io_service& _ioService;
        std::optional<boost::asio::io_service::work> _work;
        std::vector<std::thread> _threads;
    };
} // namespace lms::core