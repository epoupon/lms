#ifndef DATABASE_MEDIA_DIRECTORY_HPP
#define DATABASE_MEDIA_DIRECTORY_HPP

#include <vector>

#include <boost/filesystem/path.hpp>

#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/WtSqlTraits>

namespace Database {

class MediaDirectory;

class MediaDirectorySettings
{
	public:

		typedef Wt::Dbo::ptr<MediaDirectorySettings> pointer;

		MediaDirectorySettings() {}

		// accessors
		static pointer get(Wt::Dbo::Session& session);

		// write accessors
		void	addMediaDirectory(Wt::Dbo::ptr<MediaDirectory> mediaDirectory);
		void	setLastUpdate(boost::posix_time::ptime time)	{ _lastUpdate = time; }
		void	setLastScan(boost::posix_time::ptime time)	{ _lastScan = time; }

		// Read accessors
		boost::posix_time::ptime	getLastUpdated(void) const		{ return _lastUpdate; }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _lastUpdate,		"last_update");
				Wt::Dbo::field(a, _lastScan,		"last_scan");
				Wt::Dbo::hasMany(a, _mediaDirectories, Wt::Dbo::ManyToOne, "media_directory_settings");
			}

	private:

		boost::posix_time::time_duration	_updatePeriod;		// TODO
		boost::posix_time::ptime		_lastUpdate;		// last time the database has changed
		boost::posix_time::ptime		_lastScan;		// last time the database has been scanned
		Wt::Dbo::collection< Wt::Dbo::ptr<MediaDirectory> > _mediaDirectories;	// list of media directories
};


class MediaDirectory
{
	public:

		typedef Wt::Dbo::ptr<MediaDirectory> pointer;

		enum Type {
			Audio = 1,
			Video = 2,
		};

		MediaDirectory() {}
		MediaDirectory(boost::filesystem::path p, Type type);

		// Accessors
		static std::vector<MediaDirectory::pointer>	getAll(Wt::Dbo::Session& session);

		Type			getType(void) const	{ return _type; }
		boost::filesystem::path	getPath(void) const	{ return boost::filesystem::path(_path); }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _type,		"type");
				Wt::Dbo::field(a, _path,		"path");
				Wt::Dbo::belongsTo(a, _settings, "media_directory_settings", Wt::Dbo::OnDeleteCascade);
			}

	private:

		Type		_type;
		std::string	_path;

		MediaDirectorySettings::pointer		_settings;		// back pointer

};

} // namespace Database

#endif

