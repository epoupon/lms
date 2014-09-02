
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/asio/placeholders.hpp>

#include "logger/Logger.hpp"

#include "database/MediaDirectory.hpp"
#include "database/AudioTypes.hpp"

#include "Checksum.hpp"
#include "DatabaseUpdater.hpp"

namespace {

boost::gregorian::date
getNextDay(const boost::gregorian::date& current)
{
	boost::gregorian::day_iterator it(current);
	return *(++it);
}

boost::gregorian::date
getNextMonday(const boost::gregorian::date& current)
{
	boost::gregorian::day_iterator it(current);

	++it;
	// While it's not monday
	while( it->day_of_week() != 1 )
		++it;

	return *(it);
}

boost::gregorian::date
getNextFirstOfMonth(const boost::gregorian::date& current)
{
	boost::gregorian::day_iterator it(current);

	++it;
	// While it's not the 1st of the month
	while( it->day() != 1 )
		++it;

	return (*it);
}


bool
isFileSupported(const boost::filesystem::path& file, const std::vector<boost::filesystem::path> extensions)
{

	boost::filesystem::path fileExtension = file.extension();

	BOOST_FOREACH(const boost::filesystem::path extension, extensions)
	{
		if (extension == fileExtension)
			return true;
	}

	return false;
}

std::vector<boost::filesystem::path>
getRootDirectoriesByType(Wt::Dbo::Session& session, Database::MediaDirectory::Type type)
{
	std::vector<boost::filesystem::path> res;
	std::vector<Database::MediaDirectory::pointer> rootDirs = Database::MediaDirectory::getByType(session, type);

	BOOST_FOREACH(Database::MediaDirectory::pointer rootDir, rootDirs)
		res.push_back(rootDir->getPath());

	return res;
}

} // namespace


namespace DatabaseUpdater {

using namespace Database;


Updater::Updater(boost::filesystem::path dbPath, MetaData::Parser& parser)
 : _running(false),
_scheduleTimer(_ioService),
_db(dbPath),
_metadataParser(parser)
{
	_ioService.setThreadCount(1);
}

void
Updater::setAudioExtensions(const std::vector<std::string>& extensions)
{
	BOOST_FOREACH(const std::string& extension, extensions)
		_audioExtensions.push_back("." + extension);
}

void
Updater::setVideoExtensions(const std::vector<std::string>& extensions)
{
	BOOST_FOREACH(const std::string& extension, extensions)
		_videoExtensions.push_back("." + extension);
}

void
Updater::start(void)
{
	_running = true;

	// post some jobs in the io_service
	processNextJob();

	_ioService.start();
}

void
Updater::stop(void)
{
	_running = false;

	// TODO cancel all jobs (timer, ...)
	_scheduleTimer.cancel();

	_ioService.stop();
}

void
Updater::processNextJob(void)
{
	Wt::Dbo::Transaction transaction(_db.getSession());

	MediaDirectorySettings::pointer settings = MediaDirectorySettings::get(_db.getSession());

	if (settings->getManualScanRequested()) {
		LMS_LOG(MOD_DBUPDATER, SEV_NOTICE) << "Manual scan requested!";
		scheduleScan( boost::posix_time::seconds(0) );
	}
	else
	{
		boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
		boost::posix_time::time_duration startTime = settings->getUpdateStartTime();

		boost::gregorian::date nextScanDate;

		switch( settings->getUpdatePeriod() )
		{
			case Database::MediaDirectorySettings::Never:
				// Nothing to do
				break;
			case Database::MediaDirectorySettings::Daily:
				if (now.time_of_day() < startTime)
					nextScanDate = now.date();
				else
					nextScanDate = getNextDay(now.date());
				break;
			case Database::MediaDirectorySettings::Weekly:
				if (now.time_of_day() < startTime && now.date().day_of_week() == 1)
					nextScanDate = now.date();
				else
					nextScanDate = getNextMonday(now.date());
				break;
			case Database::MediaDirectorySettings::Monthly:
				if (now.time_of_day() < startTime && now.date().day() == 1)
					nextScanDate = now.date();
				else
					nextScanDate = getNextFirstOfMonth(now.date());
				break;
		}

		if (!nextScanDate.is_special())
			scheduleScan( boost::posix_time::ptime (nextScanDate, settings->getUpdateStartTime() ) );
	}
}

void
Updater::scheduleScan( boost::posix_time::time_duration duration)
{
	LMS_LOG(MOD_DBUPDATER, SEV_NOTICE) << "Scheduling next scan in " << duration;
	_scheduleTimer.expires_from_now(duration);
	_scheduleTimer.async_wait( boost::bind( &Updater::process, this, boost::asio::placeholders::error) );
}

void
Updater::scheduleScan( boost::posix_time::ptime time)
{
	LMS_LOG(MOD_DBUPDATER, SEV_NOTICE) << "Scheduling next scan at " << time;
	_scheduleTimer.expires_at(time);
	_scheduleTimer.async_wait( boost::bind( &Updater::process, this, boost::asio::placeholders::error) );
}

void
Updater::process(boost::system::error_code err)
{
	if (!err)
	{
		Stats stats;

		checkAudioFiles(stats);
		// TODO video files


		typedef std::pair<boost::filesystem::path, Database::MediaDirectory::Type> RootDirectory;
		std::vector<RootDirectory> rootDirectories;
		{
			Wt::Dbo::Transaction transaction(_db.getSession());
			std::vector<MediaDirectory::pointer> mediaDirectories = MediaDirectory::getAll(_db.getSession());
			BOOST_FOREACH(MediaDirectory::pointer directory, mediaDirectories)
				rootDirectories.push_back( std::make_pair( directory->getPath(), directory->getType() ));
		}

		BOOST_FOREACH( RootDirectory rootDirectory, rootDirectories)
			processDirectory(rootDirectory.first, rootDirectory.first, rootDirectory.second, stats);

		LMS_LOG(MOD_DBUPDATER, SEV_INFO) << "Changes = " << stats.nbChanges();

		// Update database stats
		boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
		{
			Wt::Dbo::Transaction transaction(_db.getSession());

			Database::MediaDirectorySettings::pointer settings = Database::MediaDirectorySettings::get(_db.getSession());

			if (stats.nbChanges() > 0)
				settings.modify()->setLastUpdate(now);

			// Save the last scan only if it has been completed
			if (_running)
				settings.modify()->setLastScan(now);

			// If the manual scan was required we can now set it to done
			// Update only if the scan is complete!
			if (settings->getManualScanRequested() && _running)
				settings.modify()->setManualScanRequested(false);

		}

		if (_running)
			processNextJob();
	}
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
			return;

		MetaData::Items items;
		_metadataParser.parse(file, items);

		// We estimate this is a audio file if:
		// - we found a least one audio
		// - the duration is not null
		if (items.find(MetaData::AudioStreams) == items.end()
		|| boost::any_cast<std::vector<MetaData::AudioStream> >(items[MetaData::AudioStreams]).empty())
		{
			LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Skipped '" << file << "' (no audio stream found)";

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
			LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Skipped '" << file << "' (no duration or duration 0)";

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
			LMS_LOG(MOD_DBUPDATER, SEV_INFO) << "Adding '" << file << "'";
			stats.nbAdded++;
		}
		else
		{
			LMS_LOG(MOD_DBUPDATER, SEV_INFO) << "Updating '" << file << "'";
			stats.nbModified++;
		}

		assert(track);

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
		LMS_LOG(MOD_DBUPDATER, SEV_ERROR) << "Exception while parsing audio file : '" << file << "': '" << e.what() << "' => skipping!";
	}
}


void
Updater::processDirectory(const boost::filesystem::path& rootDirectory,
			const boost::filesystem::path& p,
			Database::MediaDirectory::Type type,
			Stats& stats)
{
	if (!_running)
		return;

	if (!boost::filesystem::exists(p) || !boost::filesystem::is_directory(p))
		return;

	boost::filesystem::recursive_directory_iterator itPath(rootDirectory);
	boost::filesystem::recursive_directory_iterator itEnd;
	while (itPath != itEnd)
	{
		if (!_running)
			return;

		if (boost::filesystem::is_regular(*itPath)) {
			switch( type )
			{
				case Database::MediaDirectory::Audio:
					if (isFileSupported(*itPath, _audioExtensions))
						processAudioFile( *itPath, stats );

					break;

				case Database::MediaDirectory::Video:
//					processVideoFile( rootDirectory, *itPath, stats);
					break;
			}
		}

		++itPath;
	}
}

bool
Updater::checkFile(const boost::filesystem::path& p, const std::vector<boost::filesystem::path>& rootDirs, const std::vector<boost::filesystem::path>& extensions)
{
	bool status = true;

	// For each track, make sure the the file still exists
	// and still belongs to a root directory
	if (!boost::filesystem::exists( p )
			|| !boost::filesystem::is_regular( p ) )
	{
		LMS_LOG(MOD_DBUPDATER, SEV_INFO) << "Missing file '" << p << "'";
		status = false;
	}
	else
	{
		bool foundRoot = false;
		BOOST_FOREACH(const boost::filesystem::path& rootDir, rootDirs)
		{
			if (p.string().find( rootDir.string() ) != std::string::npos)
			{
				foundRoot = true;
				break;
			}
		}

		if (!foundRoot)
		{
			LMS_LOG(MOD_DBUPDATER, SEV_INFO) << "Out of root file '" << p << "'";
			status = false;
		}
		else if (!isFileSupported(p, extensions))
		{
			LMS_LOG(MOD_DBUPDATER, SEV_INFO) << "File format no longer supported for '" << p << "'";
			status = false;
		}
	}

	return status;
}


void
Updater::checkAudioFiles( Stats& stats )
{

	LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Checking audio files...";
	Wt::Dbo::Transaction transaction(_db.getSession());

	std::vector<boost::filesystem::path> rootDirs = getRootDirectoriesByType(_db.getSession(), Database::MediaDirectory::Audio);

	LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Checking tracks...";
	typedef Wt::Dbo::collection< Wt::Dbo::ptr<Track> > Tracks;
	Tracks tracks = Track::getAll(_db.getSession());

	for (Tracks::iterator it = tracks.begin(); it != tracks.end(); ++it)
	{
		Track::pointer track = (*it);

		if (!checkFile(track->getPath(), rootDirs, _audioExtensions))
		{
			track.remove();
			stats.nbRemoved++;
		}
	}

	LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Checking Artists...";
	// Now process orphan Artists (no track)
	typedef Wt::Dbo::collection< Wt::Dbo::ptr<Artist> > Artists;
	Artists artists = Artist::getAllOrphans(_db.getSession());

	for (Artists::iterator it = artists.begin(); it != artists.end(); ++it)
	{
		LMS_LOG(MOD_DBUPDATER, SEV_INFO) << "Removing orphan artist " << (*it)->getName();
		(*it).remove();
	}

	LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Checking Releases...";
	// Now process orphan Release (no track)
	typedef Wt::Dbo::collection< Wt::Dbo::ptr<Release> > Releases;
	Releases releases = Release::getAllOrphans(_db.getSession());

	for (Releases::iterator it = releases.begin(); it != releases.end(); ++it)
	{
		LMS_LOG(MOD_DBUPDATER, SEV_INFO) << "Removing orphan release " << (*it)->getName();
		(*it).remove();
	}

	// Now process orphan Genre (no track)
	LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Checking Genres...";
	typedef Wt::Dbo::collection< Wt::Dbo::ptr<Genre> > Genres;
	Genres genres = Genre::getAll(_db.getSession());

	for (Genres::iterator it = genres.begin(); it != genres.end(); ++it)
	{
		Genre::pointer genre = (*it);

		if (genre->getTracks().size() == 0)
			genre.remove();
	}


	LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Check audio files done!";
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


/*void
Updater::refreshVideoDirectory( const boost::filesystem::path& path)
{
	LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Refreshing video directory " << path;
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
				LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Skipped '" << pathChild << "' (not regular)";
			}
		}
	}
	LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Refreshing video directory " << path << ": DONE";
}*/
/*
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
			LMS_LOG(MOD_DBUPDATER, SEV_ERROR) << "Skipped '" << file << "' (last write time match)";
			return;
		}


		LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Video, parsing file " << file;

		MetaData::Items items;
		_metadataParser.parse(file, items);

		// We estimate this is a video if:
		// - we found a least one video stream
		// - the duration is not null
		if (items.find(MetaData::VideoStreams) == items.end()
		|| boost::any_cast<std::vector<MetaData::VideoStream> >(items[MetaData::VideoStreams]).empty())
		{
			LMS_LOG(MOD_DBUPDATER, SEV_ERROR) << "Skipped '" << file << "' (no video stream found)";

			// If Track exists here, delete it!
			if (dbPath)
				dbPath.remove();
		}
		else if (items.find(MetaData::Duration) == items.end()
		|| boost::any_cast<boost::posix_time::time_duration>(items[MetaData::Duration]).total_seconds() == 0)
		{
			LMS_LOG(MOD_DBUPDATER, SEV_ERROR) << "Skipped '" << file << "' (no duration or duration 0)";

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
				LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Adding '" << file << "'";
			}
			else
				LMS_LOG(MOD_DBUPDATER, SEV_DEBUG) << "Updating '" << file << "'";

			assert(video);

			video.modify()->setName( file.filename().string() );
			video.modify()->setDuration( boost::any_cast<boost::posix_time::time_duration>(items[MetaData::Duration]) );
		}

		transaction.commit();
	}
	catch( std::exception& e ) {
		LMS_LOG(MOD_DBUPDATER, SEV_ERROR) << "Exception while parsing video file : '" << file << "': '" << e.what() << "' => skipping!";
	}
}
*/
} // namespace DatabaseUpdater
