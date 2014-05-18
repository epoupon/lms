#include "AudioTypes.hpp"


Genre::Genre()
{
}

Genre::Genre(const std::string& name)
: _name( name )
{
}


Genre::pointer
Genre::getByName(Wt::Dbo::Session& session, const std::string& name)
{
	// TODO use like search
	return session.find<Genre>().where("name = ?").bind( name );
}

Genre::pointer
Genre::getNone(Wt::Dbo::Session& session)
{
	pointer res = getByName(session, "<None>");
	if (!res)
		res = create(session, "<None>");
	return res;
}

bool
Genre::isNone(void) const
{
	return (_name == "<None>");
}

Genre::pointer
Genre::create(Wt::Dbo::Session& session, const std::string& name)
{
	return session.add(new Genre(name));
}

Wt::Dbo::collection<Genre::pointer>
Genre::getAll(Wt::Dbo::Session& session, std::size_t offset, std::size_t size)
{
	return session.find<Genre>().offset(offset).limit(size);
}
