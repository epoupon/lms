
#include "RequestHandler.hpp"

namespace Remote {
namespace Server {

RequestHandler::RequestHandler(boost::filesystem::path dbPath)
: _db( dbPath )
{
}


} // namespace Server
} // namespace Remote



