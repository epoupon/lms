#include "MediaDirectory.hpp"

namespace Database {


MediaDirectorySettings::pointer
MediaDirectorySettings::get(Wt::Dbo::Session& session)
{
	MediaDirectorySettings::pointer res;

	res = session.find<MediaDirectorySettings>().where("id = ?").bind(1);
	if (!res)
		res = session.add( new MediaDirectorySettings());

	return res;
}

std::vector<MediaDirectory::pointer>
MediaDirectory::getAll(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection< MediaDirectory::pointer > res = session.find<MediaDirectory>();

	return std::vector<MediaDirectory::pointer>(res.begin(), res.end());
}

} // namespace Database
