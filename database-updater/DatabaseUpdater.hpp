#ifndef DB_UPDATER_UPDATER_HPP
#define DB_UPDATER_UPDATER_HPP


#include "metadata/MetaData.hpp"
#include "database/DatabaseHandler.hpp"

#include "database/FileTypes.hpp"
#include "database/DatabaseHandler.hpp"

namespace DatabaseUpdater {

class Updater
{
	public:

		Updater(boost::filesystem::path db, MetaData::Parser& parser);

		// Update database
		void process();

	private:

//		void refresh(const WatchedDirectory& directory);

		// Video
		void refreshVideoDirectory( const boost::filesystem::path& directory );
		void processVideoFile( const boost::filesystem::path& file);

		// Audio
		void removeMissingAudioFiles( void );
		void refreshAudioDirectory( const boost::filesystem::path& directory);
		void processAudioFile( const boost::filesystem::path& file);

		Database::Path::pointer getAddPath(const boost::filesystem::path& path);

		Database::Handler	_db;

		MetaData::Parser&		_metadataParser;
}; // class Updater

} // DatabaseUpdater

#endif
