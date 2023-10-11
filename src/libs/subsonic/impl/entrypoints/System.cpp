#include "entrypoints/System.hpp"

namespace API::Subsonic
{
    Response handlePingRequest(RequestContext& context)
    {
        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleGetLicenseRequest(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.serverProtocolVersion) };

        Response::Node& licenseNode{ response.createNode("license") };
        licenseNode.setAttribute("licenseExpires", "2025-09-03T14:46:43");
        licenseNode.setAttribute("email", "foo@bar.com");
        licenseNode.setAttribute("valid", true);

        return response;
    }

    Response handleGetOpenSubsonicExtensions(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.serverProtocolVersion) };

        {
            Response::Node& transcodeOffsetNode{ response.createArrayNode("openSubsonicExtensions") };
            transcodeOffsetNode.setAttribute("name", "transcodeOffset");
            transcodeOffsetNode.addArrayValue("versions", 1);
        }

        return response;
    };
}
