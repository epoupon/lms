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
Release::getAll(Wt::Dbo::Session& session, Artist::id_type id, int offset, int size)
{
	return session.query<Release::pointer>("select * from Release INNER JOIN Release.id = Artist.id").where("artist.id = ?").bind(id);
}

