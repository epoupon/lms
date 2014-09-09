#include <boost/foreach.hpp>

#include <Wt/WText>
#include <Wt/WPushButton>

#include <Wt/WMediaPlayer>
#include <Wt/WFileResource>

#include "transcode/Parameters.hpp"
#include "database/VideoTypes.hpp"
#include "database/MediaDirectory.hpp"

#include "VideoDatabaseWidget.hpp"

namespace UserInterface {

VideoDatabaseWidget::VideoDatabaseWidget(Database::Handler& db,  Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent),
 _db(db)
{
	_table = new Wt::WTable( this );
	_table->setHeaderCount(1);

	_table->addStyleClass("table form-inline");

	_table->toggleStyleClass("table-hover", true);
	_table->toggleStyleClass("table-striped", true);

	updateView( boost::filesystem::path(), 0);
}

void
VideoDatabaseWidget::addHeader(void)
{
	_table->elementAt(0, 0)->addWidget(new Wt::WText("Name"));
	_table->elementAt(0, 1)->addWidget(new Wt::WText("Duration"));
	_table->elementAt(0, 2)->addWidget(new Wt::WText("Action"));
}

void
VideoDatabaseWidget::addDirectory(const std::string& name, boost::filesystem::path path, size_t depth)
{

	int row = _table->rowCount();

	new Wt::WText(Wt::WString::fromUTF8( name ), _table->elementAt(row, 0));
	new Wt::WText(Wt::WString::fromUTF8( " " ), _table->elementAt(row, 1));

	Wt::WPushButton* btn = new Wt::WPushButton(Wt::WString::fromUTF8( "Open"), _table->elementAt(row, 2));

	btn->clicked().connect(std::bind([=] () {
		updateView(path, depth);
	}));

}


void
VideoDatabaseWidget::addVideo(const std::string& name, const boost::posix_time::time_duration& duration, const boost::filesystem::path& path)
{

	int row = _table->rowCount();

	new Wt::WText(Wt::WString::fromUTF8( name ), _table->elementAt(row, 0));
	new Wt::WText(Wt::WString::fromUTF8( boost::posix_time::to_simple_string( duration )), _table->elementAt(row, 1));

	Wt::WPushButton* btn = new Wt::WPushButton(Wt::WString::fromUTF8( "Play"), _table->elementAt(row, 2));

	btn->clicked().connect(std::bind([=] () {
		playVideo().emit(path);
	}));
}


void
VideoDatabaseWidget::updateView(boost::filesystem::path directory, size_t depth)
{
	_table->clear();
	addHeader();

	// If directory is not valid, add the root Media Directories
	if (depth == 0)
	{
		Wt::Dbo::Transaction transaction ( _db.getSession() );

		std::vector<Database::MediaDirectory::pointer> dirs
			= Database::MediaDirectory::getByType(_db.getSession(), Database::MediaDirectory::Video);

		BOOST_FOREACH(Database::MediaDirectory::pointer dir, dirs)
		{
			addDirectory( dir->getPath().filename().string(),
					dir->getPath(),
					depth + 1);
		}
	}
	else
	{
		addDirectory( "..", directory.parent_path(), depth - 1);

		// Iterators over files in the directory
		// If directory, add a directory entry
		// If video, add a video entry
		std::vector<boost::filesystem::path> paths;
		std::copy(boost::filesystem::directory_iterator(directory), boost::filesystem::directory_iterator(), std::back_inserter(paths));

		std::sort(paths.begin(), paths.end());

		BOOST_FOREACH(const boost::filesystem::path& path, paths)
		{
			if (boost::filesystem::is_directory(path))
				addDirectory( path.filename().string(), path, depth + 1);
			else if (boost::filesystem::is_regular(path) )
			{
				Wt::Dbo::Transaction transaction ( _db.getSession() );

				Database::Video::pointer video = Database::Video::getByPath( _db.getSession(), path);
				if (video)
					addVideo( video->getName(), video->getDuration(), path);
			}
		}
	}

}


} // namespace UserInterface

