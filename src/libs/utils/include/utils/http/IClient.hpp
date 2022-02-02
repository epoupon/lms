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

#include <string_view>
#include <boost/asio/io_context.hpp>

#include "utils/http/ClientRequestParameters.hpp"

namespace Http
{
	class IClient
	{
		public:
			virtual ~IClient() = default;

			virtual void sendGETRequest(ClientGETRequestParameters&& request) = 0;
			virtual void sendPOSTRequest(ClientPOSTRequestParameters&& request) = 0;
	};

	std::unique_ptr<IClient> createClient(boost::asio::io_context& ioContext, std::string_view baseUrl);
} // namespace Http

