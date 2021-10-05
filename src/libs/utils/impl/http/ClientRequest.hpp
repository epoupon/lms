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

#include <memory>
#include <variant>
#include "utils/http/ClientRequestParameters.hpp"

namespace Http
{
	class ClientRequest
	{
		public:
			ClientRequest(ClientGETRequestParameters&& GETParams) : _parameters {std::move(GETParams)} {}
			ClientRequest(ClientPOSTRequestParameters&& POSTParams) : _parameters {std::move(POSTParams)} {}

			std::size_t retryCount {};

			const ClientRequestParameters& getParameters() const
			{
				const ClientRequestParameters* res;

				std::visit([&](const auto& parameters)
				{
					res = &static_cast<const ClientRequestParameters&>(parameters);
				}, _parameters);

				return *res;
			}


			enum class Type
			{
				GET,
				POST
			};
			Type getType() const
			{
				if (std::holds_alternative<ClientGETRequestParameters>(_parameters))
					return Type::GET;
				else
					return Type::POST;
			}

			const ClientGETRequestParameters& getGETParameters() const
			{
				return std::get<ClientGETRequestParameters>(_parameters);
			}

			const ClientPOSTRequestParameters& getPOSTParameters() const
			{
				return std::get<ClientPOSTRequestParameters>(_parameters);
			}

		private:
			std::variant<ClientGETRequestParameters, ClientPOSTRequestParameters> _parameters;
	};
}
