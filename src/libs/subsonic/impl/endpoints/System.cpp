#include "endpoints/System.hpp"

namespace lms::api::subsonic
{
    Response handlePingRequest(RequestContext& context)
    {
        return Response::createOkResponse(context.serverProtocolVersion);
    }

    Response handleGetLicenseRequest(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.serverProtocolVersion) };

        Response::Node& licenseNode{ response.createNode("license") };
        licenseNode.setAttribute("licenseExpires", "2035-09-03T14:46:43");
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

        {
            Response::Node& formPostNode{ response.createArrayNode("openSubsonicExtensions") };
            formPostNode.setAttribute("name", "formPost");
            formPostNode.addArrayValue("versions", 1);
        }

        {
            Response::Node& songLyricsNode{ response.createArrayNode("openSubsonicExtensions") };
            songLyricsNode.setAttribute("name", "songLyrics");
            songLyricsNode.addArrayValue("versions", 1);
        }

        {
            Response::Node& apiKeyAuthentication{ response.createArrayNode("openSubsonicExtensions") };
            apiKeyAuthentication.setAttribute("name", "apiKeyAuthentication");
            apiKeyAuthentication.addArrayValue("versions", 1);
        }

        return response;
    };
} // namespace lms::api::subsonic
