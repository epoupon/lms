#ifndef _VIDEO_TYPES_HPP
#define _VIDEO_TYPES_HPP

#include <string>

#include <boost/filesystem.hpp>

#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/WtSqlTraits>

#include <Wt/WDateTime>

class Path;

class Video
{
	public:

		typedef Wt::Dbo::ptr<Video> pointer;

		Video();
		Video( Wt::Dbo::ptr<Path> path);

		// Find utilities
		static pointer getByPath(Wt::Dbo::Session& session, Wt::Dbo::ptr<Path> path);
		static Wt::Dbo::collection< pointer > getAll(Wt::Dbo::Session& session);

		// Create utility
		static pointer	create(Wt::Dbo::Session& session, Wt::Dbo::ptr<Path> path);

		// Modifiers
		void setName(const std::string& name)				{ _name = name; }
		void setDuration(boost::posix_time::time_duration duration)	{ _duration = duration; }

		// Accessors
		std::string 		getName(void) const			{ return _name; }
		Wt::Dbo::ptr<Path>	getPath(void)				{ return _path; }
		boost::posix_time::time_duration	getDuration(void) const	{ return _duration; }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _name,		"name");
				Wt::Dbo::field(a, _duration,		"duration");
	    			Wt::Dbo::belongsTo(a, _path,	"path");
			}

	private:

  		Wt::Dbo::ptr<Path>			_path;
		std::string				_name;
		boost::posix_time::time_duration	_duration;

}; // Video



#endif

