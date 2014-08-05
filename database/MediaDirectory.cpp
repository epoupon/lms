#include <boost/foreach.hpp>

#include "MediaDirectory.hpp"

namespace Database {

MediaDirectory::MediaDirectory(boost::filesystem::path p, Type type)
: _type(type),
 _path(p.string())
{
}

MediaDirectorySettings::pointer
MediaDirectorySettings::get(Wt::Dbo::Session& session)
{
	MediaDirectorySettings::pointer res;

	res = session.find<MediaDirectorySettings>().where("id = ?").bind(1);
	 // TODO bind necessary?
	if (!res)
		res = session.add( new MediaDirectorySettings());

	return res;
}

MediaDirectory::pointer
MediaDirectory::create(Wt::Dbo::Session& session, boost::filesystem::path p, Type type)
{
	return session.add( new MediaDirectory( p, type ) );
}

void
MediaDirectory::eraseAll(Wt::Dbo::Session& session)
{
	std::vector<MediaDirectory::pointer> dirs = getAll(session);
	BOOST_FOREACH(MediaDirectory::pointer dir, dirs)
		dir.remove();
}

std::vector<MediaDirectory::pointer>
MediaDirectory::getAll(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection< MediaDirectory::pointer > res = session.find<MediaDirectory>();

	return std::vector<MediaDirectory::pointer>(res.begin(), res.end());
}
MediaDirectory::pointer
MediaDirectory::getByPath(Wt::Dbo::Session& session, boost::filesystem::path p)
{
	return session.find<MediaDirectory>().where("path = ?").bind( p.string() );
}

} // namespace Database
