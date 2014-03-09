
#include "FileTypes.hpp"

Path::Path()
{}


Path::Path(boost::filesystem::path p)
: _filePath (p.string()),
 _isDirectory( boost::filesystem::is_directory( _filePath ) )
{}

Path::pointer
Path::create(Wt::Dbo::Session& session, boost::filesystem::path p)
{
	return session.add(new Path(p));
}

Path::pointer
Path::create(Wt::Dbo::Session& session, boost::filesystem::path p, Path::pointer parent)
{
	Path::pointer res = session.add(new Path(p));
	if (parent)
		parent.modify()->addChild( res );
	return res;
}

void
Path::addChild( pointer child)
{
	_childPathes.insert(child);
}

Path::pointer
Path::getByPath(Wt::Dbo::Session& session, boost::filesystem::path p)
{
	return session.find<Path>().where("path = ?").bind( p.string() );
}

std::vector<Path::pointer>
Path::getChilds(void) const
{
	std::vector< pointer > childs;

	// Get childs in another way: order by type then name
	Wt::Dbo::Query< Path::pointer> query = _childPathes.find().orderBy("path.directory DESC, path.path");
	Wt::Dbo::collection< Path::pointer > res = query.resultList();

	std::copy( res.begin(), res.end(), std::back_inserter( childs ));
	return childs;
}

Path::pointer
Path::getParent(void) const
{
	Wt::Dbo::collection< Wt::Dbo::ptr<Path> >::const_iterator it = _parentPathes.begin();
	return it != _parentPathes.end() ? *it : Path::pointer();
}

std::vector<Path::pointer>
Path::getRoots(Wt::Dbo::Session& session)
{
	std::vector<pointer> roots;

	typedef Wt::Dbo::collection< pointer > Pathes;

	Pathes pathes = session.find<Path>();

	for (Pathes::const_iterator it = pathes.begin(); it != pathes.end(); ++it)
	{
		if (!(*it)->getParent())
		{
			std::cout << "path '" << (*it)->getPath() << "' has no parent!" << std::endl;
			roots.push_back( *it );
		}
	}

	return roots;
}

Wt::Dbo::ptr<Video>
Path::getVideo()
{
	Wt::Dbo::collection< Wt::Dbo::ptr<Video> >::const_iterator it = _video.begin();
	return it != _video.end() ? *it : Wt::Dbo::ptr<Video>();
}

const Wt::Dbo::ptr<Video>
Path::getVideo() const
{
	Wt::Dbo::collection< Wt::Dbo::ptr<Video> >::const_iterator it = _video.begin();
	return it != _video.end() ? *it : Wt::Dbo::ptr<Video>();
}

std::string
Path::getFileName(void) const
{
	if (isDirectory())
		return boost::filesystem::path(_filePath).string();
	else
		return boost::filesystem::path(_filePath).filename().string();
}
