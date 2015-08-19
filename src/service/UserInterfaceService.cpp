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

#include <Wt/Dbo/SqlConnectionPool>

#include "logger/Logger.hpp"

#include "UserInterfaceService.hpp"
#include "ui/LmsApplication.hpp"

#include "config/ConfigReader.hpp"

namespace Service {

UserInterfaceService::UserInterfaceService( boost::filesystem::path runAppPath, Wt::Dbo::SqlConnectionPool& connectionPool)
: _server(runAppPath.string())
{
	std::vector<std::string> args;

	args.push_back(runAppPath.string());
	args.push_back("--docroot=" + ConfigReader::instance().getString("ui.resources.docroot"));
	args.push_back("--approot=" + ConfigReader::instance().getString("ui.resources.approot"));
	args.push_back("--https-port=" + std::to_string( ConfigReader::instance().getULong("ui.listen-endpoint.port")));
	args.push_back("--https-address=" + ConfigReader::instance().getString("ui.listen-endpoint.addr"));
	args.push_back("--ssl-certificate=" + ConfigReader::instance().getString("ui.ssl-crypto.cert"));
	args.push_back("--ssl-private-key=" + ConfigReader::instance().getString("ui.ssl-crypto.key"));
	args.push_back("--ssl-tmp-dh=" + ConfigReader::instance().getString("ui.ssl-crypto.dh"));

	// Construct argc/argv
	int argc = args.size();
	const char* argv[args.size()];
	for (int i = 0; i < argc; ++i)
		argv[i] = args[i].c_str();

	for(int i = 0; i < argc; ++i)
	{
		LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "i = " << i << ", arg = '" << argv[i] << "'";
	}

	_server.setServerConfiguration (argc, const_cast<char**>(argv));

	// bind entry point
	_server.addEntryPoint(Wt::Application, boost::bind(UserInterface::LmsApplication::create, _1, boost::ref(connectionPool)));

}

void
UserInterfaceService::start(void)
{
	_server.start();
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "UserInterfaceService::start -> Service started...";
}

void
UserInterfaceService::stop(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "UserInterfaceService::stop -> stopping...";
	_server.stop();
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "UserInterfaceService::stop -> stopped!";
}

void
UserInterfaceService::restart(void)
{

}

} // namespace Service

