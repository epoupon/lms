#include "AudioTypes.hpp"

namespace Database
{

Artist::Artist(const std::string& name)
: _name(name)
{
}

// Accesoors
Artist::pointer
Artist::getByName(Wt::Dbo::Session& session, const std::string& name)
{
	return session.find<Artist>().where("name = ?").bind( name );
}

// Create
Artist::pointer
Artist::create(Wt::Dbo::Session& session, const std::string& name)
{
	return session.add( new Artist( name ) );
}

Artist::pointer
Artist::getNone(Wt::Dbo::Session& session)
{
	pointer res = getByName(session, "<None>");
	if (!res)
		res = create(session, "<None>");

	return res;
}

Wt::Dbo::collection<Artist::pointer>
Artist::getAll(Wt::Dbo::Session& session, int offset, int size)
{
	return session.find<Artist>().offset(offset).limit(size);
}

Wt::Dbo::collection<Artist::pointer>
Artist::getAllOrphans(Wt::Dbo::Session& session)
{
	return session.query< Wt::Dbo::ptr<Artist> >("select a from artist a LEFT OUTER JOIN Track t ON a.id = t.artist_id WHERE t.id IS NULL");
}

} // namespace Database

