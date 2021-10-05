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
	createClient(boost::asio::io_context& ioContext)
	{
		return std::make_unique<Client>(ioContext);
	}

	void
	Client::sendGETRequest(ClientGETRequestParameters&& GETParams)
	{
		SendQueue& sendQueue {getOrCreateSendQueue(GETParams.url)};
		sendQueue.sendRequest(std::make_unique<ClientRequest>(std::move(GETParams)));
	}

	void
	Client::sendPOSTRequest(ClientPOSTRequestParameters&& POSTParams)
	{
		SendQueue& sendQueue {getOrCreateSendQueue(POSTParams.url)};
		sendQueue.sendRequest(std::make_unique<ClientRequest>(std::move(POSTParams)));
	}

	SendQueue&
	Client::getOrCreateSendQueue(const std::string& url)
	{
		Wt::Http::Client::URL parsedURL;
		if (!Wt::Http::Client::parseUrl(url, parsedURL))
			throw LmsException {"Cannot parse URL '" + url + "'"};

		{
			std::shared_lock lock {_sendQueuesMutex};

			if (auto it = _sendQueues.find(parsedURL.host); it != std::cend(_sendQueues))
				return it->second;
		}

		{
			std::unique_lock lock {_sendQueuesMutex};

			if (auto it = _sendQueues.find(parsedURL.host); it != std::cend(_sendQueues))
				return it->second;

			auto [it, inserted] {_sendQueues.emplace(parsedURL.host, _ioContext)};
			assert(inserted);

			return it->second;
		}
	}

} // namespace Http

