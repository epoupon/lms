#ifndef DATABASE_FILE_TYPES_HPP
#define DATABASE_FILE_TYPES_HPP

#include <string>

#include <boost/filesystem.hpp>

#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/WtSqlTraits>

#include <Wt/WDateTime>

#include "VideoTypes.hpp"

class Path
{
	public:

		typedef Wt::Dbo::ptr<Path> pointer;

		Path();
		Path(boost::filesystem::path p);

		// utility
		static pointer		create(Wt::Dbo::Session& session, boost::filesystem::path p);
		static pointer		create(Wt::Dbo::Session& session, boost::filesystem::path p, Path::pointer parent);
		static pointer		getByPath(Wt::Dbo::Session& session, boost::filesystem::path p);
		static std::vector< pointer >	getRoots(Wt::Dbo::Session& session);	// get root pathes (no parent!)

		// Modifiers
		void			addChild( pointer child );
		void setLastWriteTime(boost::posix_time::ptime time)		{ _fileLastWrite = time; }
		void setChecksum(const std::vector<unsigned char>& checksum)	{ _fileChecksum = checksum; }
		void setCreationTime(const boost::posix_time::ptime& time)	{ _creationTime = time; }

		// Accessors
		std::string		getFileName(void) const;
		boost::filesystem::path	getPath(void) const	{return boost::filesystem::path(_filePath);}
		bool			isDirectory() const	{return _isDirectory;}
		std::vector< pointer >	getChilds() const;
		pointer			getParent() const;
		boost::posix_time::ptime	getLastWriteTime(void) const	{ return _fileLastWrite; }
		const std::vector<unsigned char>& getChecksum(void) const	{ return _fileChecksum; }
		Wt::Dbo::ptr<Video>	getVideo();
		const Wt::Dbo::ptr<Video>	getVideo() const;

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _filePath,		"path");
				Wt::Dbo::field(a, _isDirectory,		"directory");
				Wt::Dbo::field(a, _creationTime,	"creation_time");
				Wt::Dbo::field(a, _fileLastWrite,	"last_write");
				Wt::Dbo::field(a, _fileChecksum,	"checksum");
				Wt::Dbo::hasMany(a, _childPathes, Wt::Dbo::ManyToMany, "path_path", "child_path_id");
				Wt::Dbo::hasMany(a, _parentPathes, Wt::Dbo::ManyToMany, "path_path", "parent_path_id");
				Wt::Dbo::hasMany(a, _video, Wt::Dbo::ManyToOne, "path");
			}
	private:

		std::string			_filePath;
		bool				_isDirectory;
		boost::posix_time::ptime	_creationTime;
		std::vector<unsigned char>	_fileChecksum;
		boost::posix_time::ptime	_fileLastWrite;

		Wt::Dbo::collection< Wt::Dbo::ptr<Video> > 	_video;

		Wt::Dbo::collection< Wt::Dbo::ptr<Path> >	_childPathes;	// Child pathes
		Wt::Dbo::collection< Wt::Dbo::ptr<Path> >	_parentPathes;	// Parent pathes

};


#endif

