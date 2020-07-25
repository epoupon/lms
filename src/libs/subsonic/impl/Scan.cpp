/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "Scan.hpp"

#include "scanner/IMediaScanner.hpp"
#include "utils/Service.hpp"

namespace API::Subsonic::Scan
{
	using namespace Scanner;

	static
	Response::Node
	createStatusResponseNode()
	{
		Response::Node statusResponse;

		const IMediaScanner::Status scanStatus {ServiceProvider<IMediaScanner>::get()->getStatus()};

		statusResponse.setAttribute("scanning", scanStatus.currentState == IMediaScanner::State::InProgress);
		if (scanStatus.currentState == IMediaScanner::State::InProgress && scanStatus.inProgressScanStats)
			statusResponse.setAttribute("count", scanStatus.inProgressScanStats->processedFiles);

		return statusResponse;
	}


	Response
	handleGetScanStatus(RequestContext& context)
	{
		Response response {Response::createOkResponse(context)};
		response.addNode("scanStatus", createStatusResponseNode());

		return response;
	}

	Response
	handleStartScan(RequestContext& context)
	{
		ServiceProvider<IMediaScanner>::get()->requestImmediateScan(false);

		Response response {Response::createOkResponse(context)};
		response.addNode("scanStatus", createStatusResponseNode());

		return response;
	}
}

