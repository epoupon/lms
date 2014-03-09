#include <boost/foreach.hpp>

#include "AudioTypes.hpp"


Track::Track(const boost::filesystem::path& p, Artist::pointer artist, Release::pointer release)
:
_trackNumber(0),
_discNumber(0),
_filePath( p.string() ),
_artist(artist),
_release(release)
{
}


void
Track::setGenres(std::vector<Genre::pointer> genres)
{
	if (_genres.size())
		_genres.clear();

	BOOST_FOREACH(Genre::pointer genre, genres) {
		_genres.insert( genre );
	}
}

Track::pointer
Track::getByPath(Wt::Dbo::Session& session, const boost::filesystem::path& p)
{
	Wt::Dbo::Transaction transaction(session);

	return session.find<Track>().where("path = ?").bind(p.string());
}


Track::pointer
Track::create(Wt::Dbo::Session& session, const boost::filesystem::path& p, Artist::pointer artist, Release::pointer release)
{
	return session.add(new Track(p, artist, release) );
}

Wt::Dbo::collection< Track::pointer >
Track::getAll(Wt::Dbo::Session& session)
{
	return session.find<Track>();
}

std::vector< Genre::pointer >
Track::getGenres(void) const
{
	std::vector< Genre::pointer > genres;
	std::copy(_genres.begin(), _genres.end(), std::back_inserter(genres));
	return genres;
}
