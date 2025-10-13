#include "endpoints/System.hpp"

namespace lms::api::subsonic
{
    Response handlePingRequest(RequestContext& context)
    {
        return Response::createOkResponse(context.getServerProtocolVersion());
    }

    Response handleGetLicenseRequest(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };

        Response::Node& licenseNode{ response.createNode("license") };
        licenseNode.setAttribute("licenseExpires", "2035-09-03T14:46:43");
        licenseNode.setAttribute("email", "foo@bar.com");
        licenseNode.setAttribute("valid", true);

        return response;
    }

    Response handleGetOpenSubsonicExtensions(RequestContext& context)
    {
        Response response{ Response::createOkResponse(context.getServerProtocolVersion()) };

        struct Extension
        {
            core::LiteralString name;
            int version;
        };

        constexpr Extension extensions[] = {
            { "transcodeOffset", 1 },
            { "formPost", 1 },
            { "songLyrics", 1 },
            { "apiKeyAuthentication", 1 },
            { "getPodcastEpisode", 1 },
            { "transcoding", 1 },
        };

        for (const Extension& extension : extensions)
        {
            Response::Node& extensionNode{ response.createArrayNode("openSubsonicExtensions") };
            extensionNode.setAttribute("name", extension.name.str());
            extensionNode.addArrayValue("versions", extension.version);
        }

        return response;
    };
} // namespace lms::api::subsonic
