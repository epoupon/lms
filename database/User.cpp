
#include "User.hpp"


namespace Database {

User::User()
: _maxAudioBitrate(maxAudioBitrate),
 _maxVideoBitrate(maxVideoBitrate),
_isAdmin(false),
_audioBitrate(defaultAudioBitrate),
_videoBitrate(defaultVideoBitrate)
{

}

std::vector<User::pointer>
User::getAll(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection<pointer> res = session.find<User>();
	return std::vector<pointer>(res.begin(), res.end());
}



} // namespace Database


