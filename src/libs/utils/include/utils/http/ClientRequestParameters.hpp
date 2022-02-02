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

#include <functional>
#include <string_view>
#include <vector>

#include <Wt/Http/Message.h>

namespace Http
{
	struct ClientRequestParameters
	{
		enum class Priority
		{
			High,
			Normal,
			Low,
		};

		Priority	priority {Priority::Normal};
		std::string relativeUrl; // relative to baseUrl used by the client

		using OnSuccessFunc = std::function<void(std::string_view msgBody)>;
		OnSuccessFunc onSuccessFunc;

		using OnFailureFunc = std::function<void()>;
		OnFailureFunc onFailureFunc;
	};

	struct ClientGETRequestParameters final : public ClientRequestParameters
	{
		std::vector<Wt::Http::Message::Header> headers;
	};

	struct ClientPOSTRequestParameters final : public ClientRequestParameters
	{
		Wt::Http::Message message;
	};

} // namespace Http

