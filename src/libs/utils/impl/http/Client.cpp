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

#include "Client.hpp"
#include "utils/Exception.hpp"

namespace Http
{
	std::unique_ptr<IClient>
	createClient(boost::asio::io_context& ioContext, std::string_view baseUrl)
	{
		return std::make_unique<Client>(ioContext, baseUrl);
	}

	void
	Client::sendGETRequest(ClientGETRequestParameters&& GETParams)
	{
		_sendQueue.sendRequest(std::make_unique<ClientRequest>(std::move(GETParams)));
	}

	void
	Client::sendPOSTRequest(ClientPOSTRequestParameters&& POSTParams)
	{
		_sendQueue.sendRequest(std::make_unique<ClientRequest>(std::move(POSTParams)));
	}
} // namespace Http

