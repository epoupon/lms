
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>

#include "database/MediaDirectory.hpp"
#include "database/AudioTypes.hpp"

#include "Checksum.hpp"
#include "DatabaseUpdater.hpp"

namespace DatabaseUpdater {

using namespace Database;

Updater::Updater(boost::filesystem::path dbPath, MetaData::Parser& parser)
 : _db(dbPath),
   _metadataParser(parser)
{

}

void
Updater::process(void)
{
	removeMissingAudioFiles(_result.audioStats);
	// TODO video files

	Wt::Dbo::Transaction transaction(_db.getSession());

	std::vector<MediaDirectory::pointer> mediaDirectories = MediaDirectory::getAll(_db.getSession());

	BOOST_FOREACH( MediaDirectory::pointer directory, mediaDirectories)
	{
		switch (directory->getType()) {
			case MediaDirectory::Audio:
				refreshAudioDirectory(directory->getPath(), _result.audioStats);
				break;
			case MediaDirectory::Video:
				refreshVideoDirectory(directory->getPath());
				break;
		}
	}

	std::cout << "Audio changes = " << _result.audioStats.nbChanges() << std::endl;
	std::cout << "Video changes = " << _result.videoStats.nbChanges() << std::endl;

	Database::MediaDirectorySettings::pointer settings = Database::MediaDirectorySettings::get(_db.getSession());
	boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();

	if (_result.audioStats.nbChanges() + _result.videoStats.nbChanges() > 0)
		settings.modify()->setLastUpdate(now);

	settings.modify()->setLastScan(now);

	transaction.commit();
}

void
Updater::processAudioFile( const boost::filesystem::path& file, Stats& stats)
{
	try {

		// Check last update time
	        boost::posix_time::ptime lastWriteTime (boost::posix_time::from_time_t( boost::filesystem::last_write_time( file ) ) );

		Wt::Dbo::Transaction transaction(_db.getSession());

		// Skip file if last write is the same
		Wt::Dbo::ptr<Track> track = Track::getByPath(_db.getSession(), file);
		if (track && track->getLastWriteTime() == lastWriteTime)
		{
			std::cerr << "Skipped '" << file << "' (last write time match)" << std::endl;
			return;
		}

		std::vector<unsigned char> checksum;
		computeCrc( file, checksum );

		// Skip file if its checksum is still the same
		if (track && track->getChecksum() == checksum) {
			std::cerr << "Skipped '" << file << "' (checksum match)" << std::endl;
			return;
		}

		std::cout << "parsing file " << file << std::endl;

		MetaData::Items items;
		_metadataParser.parse(file, items);

		// We estimate this is a audio file if:
		// - we found a least one audio
		// - the duration is not null
		if (items.find(MetaData::AudioStreams) == items.end()
		|| boost::any_cast<std::vector<MetaData::AudioStream> >(items[MetaData::AudioStreams]).empty())
		{
			std::cerr << "Skipped '" << file << "' (no audio stream found)" << std::endl;

			// If Track exists here, delete it!
			if (track) {
				track.remove();
				stats.nbRemoved++;
			}
			return;
		}
		if (items.find(MetaData::Duration) == items.end()
		|| boost::any_cast<boost::posix_time::time_duration>(items[MetaData::Duration]).total_seconds() == 0)
		{
			std::cerr << "Skipped '" << file << "' (no duration or duration 0)" << std::endl;

			// If Track exists here, delete it!
			if (track) {
				track.remove();
				stats.nbRemoved++;
			}
			return;
		}

		std::string title;
		if (items.find(MetaData::Title) != items.end()) {
			title = boost::any_cast<std::string>(items[MetaData::Title]);
		}
		else
		{
			// TODO parse file name guess track etc.
			// For now juste use file name as title
			title = file.filename().string();
		}

		// ***** Artist
		Wt::Dbo::ptr<Artist> artist;
		if (items.find(MetaData::Artist) != items.end())
		{
			const std::string artistName (boost::any_cast<std::string>(items[MetaData::Artist]));
			artist = Artist::getByName(_db.getSession(), artistName );
			if (!artist)
				artist = Artist::create( _db.getSession(), artistName );
		}
		else
			artist = Artist::getNone(_db.getSession());

		assert(artist);


		// ***** Release
		Wt::Dbo::ptr<Release> release;
		if (items.find(MetaData::Album) != items.end())
		{
			const std::string albumName (boost::any_cast<std::string>(items[MetaData::Album]));
			release = Release::getByName(_db.getSession(), albumName);
			if (!release)
				release = Release::create( _db.getSession(),  albumName );
		}
		else
			release = Release::getNone( _db.getSession() );

		assert(release);


		// ***** Genres
		typedef std::list<std::string> GenreList;
		GenreList genreList;
		std::vector< Genre::pointer > genres;
		if (items.find(MetaData::Genres) != items.end())
		{
			genreList = (boost::any_cast<GenreList>(items[MetaData::Genres]));

			BOOST_FOREACH(const std::string& genre, genreList) {
				Genre::pointer dbGenre ( Genre::getByName(_db.getSession(), genre) );
				if (!dbGenre)
					dbGenre = Genre::create(_db.getSession(), genre);

				genres.push_back( dbGenre );
			}
		}
		if (genres.empty())
			genres.push_back( Genre::getNone( _db.getSession() ));

		assert( !genres.empty() );

		// If file already exist, update data
		// Otherwise, create it
		if (!track)
		{
			// Create a new song
			track = Track::create(_db.getSession(), file, artist, release);
			std::cout << "Adding '" << file << "'" << std::endl;
			stats.nbAdded++;
		}
		else
		{
			std::cout << "Updating '" << file << "'" << std::endl;
			stats.nbModified++;
		}

		assert(track);

		track.modify()->setChecksum(checksum);
		track.modify()->setLastWriteTime(lastWriteTime);
		track.modify()->setName(title);

		{
			std::string trackGenreList;
			// Product genre list
			BOOST_FOREACH(const std::string& genre, genreList) {
				if (!trackGenreList.empty())
					trackGenreList += ", ";
				trackGenreList += genre;
			}

			std::cout << "Genre list = " << trackGenreList << std::endl;
			track.modify()->setGenres( trackGenreList );
		}
		track.modify()->setGenres( genres );
		track.modify()->setArtist( artist );
		track.modify()->setRelease( release );

		if (items.find(MetaData::TrackNumber) != items.end())
			track.modify()->setTrackNumber( boost::any_cast<std::size_t>(items[MetaData::TrackNumber]) );

		if (items.find(MetaData::DiscNumber) != items.end())
			track.modify()->setDiscNumber( boost::any_cast<std::size_t>(items[MetaData::DiscNumber]) );

		if (items.find(MetaData::Duration) != items.end())
			track.modify()->setDuration( boost::any_cast<boost::posix_time::time_duration>(items[MetaData::Duration]) );

		if (items.find(MetaData::CreationTime) != items.end())
			track.modify()->setCreationTime( boost::any_cast<boost::posix_time::ptime>(items[MetaData::CreationTime]) );

		transaction.commit();

	}
	catch( std::exception& e ) {
		std::cerr << "Exception while parsing audio file : '" << file << "': '" << e.what() << "' => skipping!" << std::endl;
	}
}


void
Updater::refreshAudioDirectory( const boost::filesystem::path& p, Stats& stats)
{
	std::cout << "Refreshing audio directory " << p << std::endl;
	if (boost::filesystem::exists(p) && boost::filesystem::is_directory(p)) {

		typedef std::vector<boost::filesystem::path> Paths;             // store paths,

		Paths files;
		std::copy(boost::filesystem::directory_iterator(p), boost::filesystem::directory_iterator(), std::back_inserter(files));

		BOOST_FOREACH(const boost::filesystem::path& file, files) {

			boost::this_thread::interruption_point();

			try {
				if (boost::filesystem::is_directory(file)) {
					refreshAudioDirectory( file, stats );
				}
				else if (boost::filesystem::is_regular(file)) {
					processAudioFile( file, stats );
				}
				else {
					std::cout << "Skipped '" << file << "' (not regular)" << std::endl;
				}
			}
			catch(std::exception& e) {
				std::cerr << "Exception while accessing '" << file << ": " << e.what() << std::endl;
			}
		}
	}
	std::cout << "Refreshing audio directory " << p << ": DONE" << std::endl;
}

void
Updater::removeMissingAudioFiles( Stats& stats )
{
	std::cerr << "Removing missing files..." << std::endl;
	Wt::Dbo::Transaction transaction(_db.getSession());

	typedef Wt::Dbo::collection< Wt::Dbo::ptr<Track> > Tracks;

	Tracks tracks = Track::getAll(_db.getSession());

	for (Tracks::iterator i = tracks.begin(); i != tracks.end(); ++i)
	{
		const boost::filesystem::path p ((*i)->getPath() );
		if (!boost::filesystem::exists( p )
			|| !boost::filesystem::is_regular( p ) )
		{
			(*i).remove();
			stats.nbRemoved++;
			std::cerr << "Removing file '" << p << "'" << std::endl;
		}
	}

	transaction.commit();

	std::cerr << "Refreshing missing files done!" << std::endl;
}

Path::pointer
Updater::getAddPath(const boost::filesystem::path& path)
{
	Path::pointer res;
	Path::pointer parentDirectory;

	if (path.has_parent_path())
		parentDirectory = Path::getByPath(_db.getSession(), path.parent_path());

	res = Path::getByPath(_db.getSession(), path);
	if (!res)
		res = Path::create(_db.getSession(), path, parentDirectory);
	else
	{
		// Make sure the parent directory owns the child
		if (parentDirectory && !res->getParent())
			parentDirectory.modify()->addChild( res );
	}

	return res;
}


void
Updater::refreshVideoDirectory( const boost::filesystem::path& path)
{
	std::cout << "Refreshing video directory " << path << std::endl;
	if (boost::filesystem::exists(path) && boost::filesystem::is_directory(path))
	{

		// Add this directory in the database
		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			Path::pointer pathDirectory = getAddPath(path);
			assert( pathDirectory->isDirectory() );

			transaction.commit();
		}

		// Now process all files/dirs in directory
		typedef std::vector<boost::filesystem::path> Paths;             // store paths,

		Paths pathChildren;
		std::copy(boost::filesystem::directory_iterator(path), boost::filesystem::directory_iterator(), std::back_inserter(pathChildren));

		BOOST_FOREACH(const boost::filesystem::path& pathChild, pathChildren) {

			if (boost::filesystem::is_directory(pathChild)) {
				refreshVideoDirectory( pathChild );
			}
			else if (boost::filesystem::is_regular(pathChild)) {
				processVideoFile( pathChild );
			}
			else {
				std::cout << "Skipped '" << pathChild << "' (not regular)" << std::endl;
			}
		}
	}
	std::cout << "Refreshing video directory " << path << ": DONE" << std::endl;
}

void
Updater::processVideoFile( const boost::filesystem::path& file)
{
	try {

		// Check last update time
	        boost::posix_time::ptime lastWriteTime (boost::posix_time::from_time_t( boost::filesystem::last_write_time( file ) ) );

		Wt::Dbo::Transaction transaction(_db.getSession());

		// Skip file if last write is the same
		Path::pointer dbPath = Path::getByPath(_db.getSession(), file);
		if (dbPath && dbPath->getLastWriteTime() == lastWriteTime)
		{
			std::cerr << "Skipped '" << file << "' (last write time match)" << std::endl;
			return;
		}


		std::cout << "Video, parsing file " << file << std::endl;

		MetaData::Items items;
		_metadataParser.parse(file, items);

		// We estimate this is a video if:
		// - we found a least one video stream
		// - the duration is not null
		if (items.find(MetaData::VideoStreams) == items.end()
		|| boost::any_cast<std::vector<MetaData::VideoStream> >(items[MetaData::VideoStreams]).empty())
		{
			std::cerr << "Skipped '" << file << "' (no video stream found)" << std::endl;

			// If Track exists here, delete it!
			if (dbPath)
				dbPath.remove();
		}
		else if (items.find(MetaData::Duration) == items.end()
		|| boost::any_cast<boost::posix_time::time_duration>(items[MetaData::Duration]).total_seconds() == 0)
		{
			std::cerr << "Skipped '" << file << "' (no duration or duration 0)" << std::endl;

			// If Track exists here, delete it!
			if (dbPath)
				dbPath.remove();
		}
		else
		{
			// add Path if needed
			if (!dbPath)
				dbPath = getAddPath( file );

			dbPath.modify()->setLastWriteTime( lastWriteTime );

			assert(dbPath);

			// Valid video here
			// Today we are very aggressive, but we could also guess names from path, etc.

			Video::pointer video = dbPath.modify()->getVideo();
			if (!video) {
				video = Video::create(_db.getSession(), dbPath);
				std::cout << "Adding '" << file << "'" << std::endl;
			}
			else
				std::cout << "Updating '" << file << "'" << std::endl;

			assert(video);

			video.modify()->setName( file.filename().string() );
			video.modify()->setDuration( boost::any_cast<boost::posix_time::time_duration>(items[MetaData::Duration]) );
		}

		transaction.commit();
	}
	catch( std::exception& e ) {
		std::cerr << "Exception while parsing video file : '" << file << "': '" << e.what() << "' => skipping!" << std::endl;
	}
}

} // namespace DatabaseUpdater
