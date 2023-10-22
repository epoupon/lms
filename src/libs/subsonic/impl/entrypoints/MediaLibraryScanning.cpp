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

#include "MediaLibraryScanning.hpp"

#include "services/scanner/IScannerService.hpp"
#include "utils/Service.hpp"

namespace API::Subsonic::Scan
{
    using namespace Scanner;

    namespace
    {
        Response::Node createStatusResponseNode()
        {
            Response::Node statusResponse;

            const IScannerService::Status scanStatus{ Service<IScannerService>::get()->getStatus() };

            statusResponse.setAttribute("scanning", scanStatus.currentState == IScannerService::State::InProgress);
            if (scanStatus.currentState == IScannerService::State::InProgress)
            {
                std::size_t count{};

                if (scanStatus.currentScanStepStats && scanStatus.currentScanStepStats->currentStep == ScanStep::ScanningFiles)
                    count = scanStatus.currentScanStepStats->processedElems;

                statusResponse.setAttribute("count", count);
            }

            return statusResponse;
        }
    }

    Response handleGetScanStatus(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        response.addNode("scanStatus", createStatusResponseNode());

        return response;
    }

    Response handleStartScan(RequestContext& context)
    {
        Service<IScannerService>::get()->requestImmediateScan(false);

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        response.addNode("scanStatus", createStatusResponseNode());

        return response;
    }
}

