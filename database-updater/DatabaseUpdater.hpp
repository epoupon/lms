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

		struct Stats
		{
			std::size_t	nbAdded;
			std::size_t	nbRemoved;
			std::size_t	nbModified;
			Stats() : nbAdded(0), nbRemoved(0), nbModified(0) {}

			std::size_t nbChanges() const { return nbAdded + nbRemoved + nbModified;}
		};

		struct Result
		{
			Stats	audioStats;
			Stats	videoStats;
		};

		// Video
		void refreshVideoDirectory( const boost::filesystem::path& directory );
		void processVideoFile( const boost::filesystem::path& file);

		// Audio
		void removeMissingAudioFiles( Stats& stats );
		void refreshAudioDirectory( const boost::filesystem::path& directory, Stats& stats);
		void processAudioFile( const boost::filesystem::path& file, Stats& stats);

		Database::Path::pointer getAddPath(const boost::filesystem::path& path);

		Database::Handler	_db;

		MetaData::Parser&	_metadataParser;

		Result			_result;	// update results
}; // class Updater

} // DatabaseUpdater

#endif
