/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "Executor.hpp"

#include <boost/asio/io_context.hpp>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/post.hpp>

namespace lms::podcast
{
    Executor::Executor(boost::asio::io_context& ioContext)
        : _strand{ ioContext }
    {
    }

    void Executor::post(std::function<void()> callback)
    {
        boost::asio::post(boost::asio::bind_executor(_strand, std::move(callback)));
    }
} // namespace lms::podcast