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

#include "core/Service.hpp"
#include "services/scanner/IScannerService.hpp"

namespace lms::api::subsonic::Scan
{
    using namespace scanner;

    namespace
    {
        Response::Node createStatusResponseNode()
        {
            Response::Node statusResponse;

            const IScannerService::Status scanStatus{ core::Service<IScannerService>::get()->getStatus() };

            statusResponse.setAttribute("scanning", scanStatus.currentState == IScannerService::State::InProgress);
            if (scanStatus.currentState == IScannerService::State::InProgress)
            {
                std::size_t count{};

                if (scanStatus.currentScanStepStats && scanStatus.currentScanStepStats->currentStep == ScanStep::ScanFiles)
                    count = scanStatus.currentScanStepStats->processedElems;

                statusResponse.setAttribute("count", count);
            }

            return statusResponse;
        }
    } // namespace

    Response handleGetScanStatus(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        response.addNode("scanStatus", createStatusResponseNode());

        return response;
    }

    Response handleStartScan(RequestContext& context)
    {
        core::Service<IScannerService>::get()->requestImmediateScan();

        Response response{ Response::createOkResponse(context.serverProtocolVersion) };
        response.addNode("scanStatus", createStatusResponseNode());

        return response;
    }
} // namespace lms::api::subsonic::Scan
