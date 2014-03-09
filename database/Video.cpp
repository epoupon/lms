#include "VideoTypes.hpp"

#include "FileTypes.hpp"

Video::Video()
{
}

Video::Video(Wt::Dbo::ptr<Path> path)
: _path(path)
{
}

Video::pointer
Video::create(Wt::Dbo::Session& session, Wt::Dbo::ptr<Path> path)
{
	return session.add(new Video(path));
}
