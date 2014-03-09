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

