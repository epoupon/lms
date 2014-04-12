#include "SessionData.hpp"

namespace UserInterface {

SessionData::SessionData(boost::filesystem::path dbPath)
: _db(dbPath)
{
}

} // namespace UserInterface

