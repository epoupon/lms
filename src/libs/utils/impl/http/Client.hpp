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

#include <unordered_map>
#include <string>
#include <shared_mutex>

#include "utils/http/IClient.hpp"
#include "SendQueue.hpp"

namespace Http
{
	class Client final : public IClient
	{
		public:
			Client(boost::asio::io_context& ioContext, std::string_view baseUrl)
			: _sendQueue {ioContext, baseUrl}
			{}

		private:
			void sendGETRequest(ClientGETRequestParameters&& request) override;
			void sendPOSTRequest(ClientPOSTRequestParameters&& request) override;

			SendQueue					_sendQueue;
	};
} // namespace Http

