#include "VideoTypes.hpp"

namespace Database {

Video::Video()
{
}

Video::Video(const boost::filesystem::path& p)
: _filePath(p.string())
{
}

Video::pointer
Video::create(Wt::Dbo::Session& session, const boost::filesystem::path& p)
{
	return session.add(new Video(p));
}

Wt::Dbo::collection< Video::pointer >
Video::getAll(Wt::Dbo::Session& session)
{
	return session.find<Video>();
}

Video::pointer
Video::getByPath(Wt::Dbo::Session& session, const boost::filesystem::path& p)
{
	return session.find<Video>().where("path = ?").bind(p.string());
}

} // namespace Video
