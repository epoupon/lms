
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




} // namespace Database


