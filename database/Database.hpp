
#include "metadata/MetaData.hpp"

#include "DatabaseHandler.hpp"

#include "FileTypes.hpp"

class WatchedDirectory {

	public:

		enum Type {
			Audio,
			Video,
		};

		WatchedDirectory( boost::filesystem::path path, Type type) : _path(path), _type(type) {
			if (!boost::filesystem::is_directory(path))
				throw std::runtime_error( "path '" + path.string() + "' is not a directory!");
		}

		bool operator==(WatchedDirectory const& other) { return _path == other._path && _type == other._type; }

		const boost::filesystem::path& getPath() const { return _path; }
		Type	getType() const { return _type; }

	private:

		boost::filesystem::path _path;
		Type _type;
};


class Database
{
	public:

		Database(boost::filesystem::path db, MetaData::Parser& parser);

		void watchDirectory(WatchedDirectory dir);
		void unwatchDirectory(WatchedDirectory dir);

		// Rescan media folders
		void refresh();

	private:

		void refresh(const WatchedDirectory& directory);

		// Video
		void refreshVideoDirectory( const boost::filesystem::path& directory );
		void processVideoFile( const boost::filesystem::path& file);

		// Audio
		void removeMissingAudioFiles( void );
		void refreshAudioDirectory( const boost::filesystem::path& directory);
		void processAudioFile( const boost::filesystem::path& file);

		Path::pointer getAddPath(const boost::filesystem::path& path);

		DatabaseHandler	_db;

		std::list<WatchedDirectory>	_directories;
		MetaData::Parser&		_metadataParser;
};

