/*
 * Copyright (C) 2013 Emeric Poupon
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

#include "logger/Logger.hpp"

#include "LmsAPIServerService.hpp"

namespace Service {

LmsAPIService::LmsAPIService(const Config& config)
: _server(boost::asio::ip::tcp::endpoint(config.address, config.port),
	config.sslCertificatePath,
	config.sslPrivateKeyPath,
	config.sslTempDhPath,
	config.dbPath)
{
}

void
LmsAPIService::start(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "LmsAPIService::start, starting...";
	_server.start();
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "LmsAPIService::start, started!";
}


void
LmsAPIService::stop(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "LmsAPIService::stop, stopping...";
	_server.stop();
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "LmsAPIService::stop, stopped!";
}

void
LmsAPIService::restart(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "LmsAPIService::restart, not implemented!";
}

} // namespace Service
