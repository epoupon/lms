#include "AudioTypes.hpp"


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
Artist::getAll(Wt::Dbo::Session& session, std::size_t offset, std::size_t size)
{
	return session.find<Artist>().offset(offset).limit(size);
}
