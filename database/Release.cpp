#include <boost/foreach.hpp>

#include "SqlQuery.hpp"

#include "AudioTypes.hpp"


Release::Release(const std::string& name)
: _name(name)
{
}

Release::pointer
Release::getByName(Wt::Dbo::Session& session, const std::string& name)
{
	return session.find<Release>().where("name = ?").bind( name );
}

Release::pointer
Release::getNone(Wt::Dbo::Session& session)
{
	pointer res = getByName(session, "<None>");
	if (!res)
		res = create(session, "<None>");

	return res;
}

Release::pointer
Release::create(Wt::Dbo::Session& session, const std::string& name)
{
	return session.add(new Release(name));
}

Wt::Dbo::collection<Release::pointer>
Release::getAll(Wt::Dbo::Session& session, std::vector<Artist::id_type> artistIds, int offset, int size)
{
	std::string sqlQuery = "SELECT r FROM release r";

	if (!artistIds.empty())
	{
		sqlQuery += " INNER JOIN artist a ON a.id = t.artist_id";
		sqlQuery += " INNER JOIN track t ON t.release_id = r.id";
	}

	Wt::Dbo::Query<Release::pointer> query = session.query<Release::pointer>( sqlQuery ).offset(offset).limit(size);

	BOOST_FOREACH(const Artist::id_type artistId, artistIds)
		query.where("a.id = ?").bind(artistId);

	query.groupBy("r");

	return query;
}

boost::posix_time::time_duration
Release::getDuration(void) const
{
	typedef Wt::Dbo::collection< Wt::Dbo::ptr<Track> > Tracks;

	boost::posix_time::time_duration res;

	for (Tracks::const_iterator it = _tracks.begin(); it != _tracks.end(); ++it)
		res += (*it)->getDuration();

	return res;
}

