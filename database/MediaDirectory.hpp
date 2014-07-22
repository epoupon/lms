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
		void	setManualScanRequested(bool value)		{ _manualScanRequested = value;}
		void	setUpdatePeriod(boost::posix_time::time_duration dur)		{ _updatePeriod = dur;}
		void	setUpdateStartTime(boost::posix_time::time_duration dur)	{ _updateStartTime = dur;}
		void	setLastUpdate(boost::posix_time::ptime time)	{ _lastUpdate = time; }
		void	setLastScan(boost::posix_time::ptime time)	{ _lastScan = time; }

		// Read accessors
		bool				getManualScanRequested(void) const	{ return _manualScanRequested; }
		boost::posix_time::time_duration	getUpdatePeriod(void) const	{ return _updatePeriod; }
		boost::posix_time::time_duration	getUpdateStartTime(void) const	{ return _updateStartTime; }
		boost::posix_time::ptime	getLastUpdated(void) const		{ return _lastUpdate; }
		boost::posix_time::ptime	getLastScan(void) const		{ return _lastScan; }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _manualScanRequested,	"manual_scan_requested");
				Wt::Dbo::field(a, _updatePeriod,	"update_period");
				Wt::Dbo::field(a, _updateStartTime,	"update_start_time");
				Wt::Dbo::field(a, _lastUpdate,		"last_update");
				Wt::Dbo::field(a, _lastScan,		"last_scan");
				Wt::Dbo::hasMany(a, _mediaDirectories, Wt::Dbo::ManyToOne, "media_directory_settings");
			}

	private:

		bool					_manualScanRequested;	// Immadiate scan has been requested by user
		boost::posix_time::time_duration	_updatePeriod;		// How long between updates
		boost::posix_time::time_duration	_updateStartTime;	// Time of day to begin the update
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
		static pointer create(Wt::Dbo::Session& session, boost::filesystem::path p, Type type);
		static std::vector<MediaDirectory::pointer>	getAll(Wt::Dbo::Session& session);

		static void eraseAll(Wt::Dbo::Session& session);

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

