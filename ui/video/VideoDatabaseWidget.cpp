#include <boost/foreach.hpp>

#include <Wt/WText>
#include <Wt/WPushButton>

#include <Wt/WMediaPlayer>
#include <Wt/WFileResource>
#include "transcode/Parameters.hpp"

#include "resource/AvConvTranscodeStreamResource.hpp"

#include "VideoDatabaseWidget.hpp"

namespace UserInterface {

VideoDatabaseWidget::VideoDatabaseWidget(DatabaseHandler& db,  Wt::WContainerWidget *parent)
: Wt::WContainerWidget(parent),
 _db(db)
{
	_table = new Wt::WTable( this );
	_table->setHeaderCount(1);

	Wt::Dbo::Transaction transaction ( _db.getSession() );

	updateView( Path::pointer() );

	_table->addStyleClass("table form-inline");

	_table->toggleStyleClass("table-hover", true);
	_table->toggleStyleClass("table-striped", true);

}

void
VideoDatabaseWidget::addHeader(void)
{
	_table->elementAt(0, 0)->addWidget(new Wt::WText("Name"));
	_table->elementAt(0, 1)->addWidget(new Wt::WText("Duration"));
	_table->elementAt(0, 2)->addWidget(new Wt::WText("Action"));
}

void
VideoDatabaseWidget::addDirectory(const std::string& name, const boost::filesystem::path& path)
{

	int row = _table->rowCount();

	std::cout << "Adding DIR at row " << row << ", path = '" << path << "'" << std::endl;

	new Wt::WText(Wt::WString::fromUTF8( name ), _table->elementAt(row, 0));
	new Wt::WText(Wt::WString::fromUTF8( " " ), _table->elementAt(row, 1));

	Wt::WPushButton* btn = new Wt::WPushButton(Wt::WString::fromUTF8( "Open"), _table->elementAt(row, 2));
	btn->clicked().connect( boost::bind(&VideoDatabaseWidget::handleOpenDirectory, this, path) );
}


void
VideoDatabaseWidget::addVideo(const std::string& name, const boost::posix_time::time_duration& duration, const boost::filesystem::path& path)
{

	int row = _table->rowCount();

	std::cout << "Adding VIDEO at row " << row << ", path = '" << path << "'" << std::endl;

	new Wt::WText(Wt::WString::fromUTF8( name ), _table->elementAt(row, 0));
	new Wt::WText(Wt::WString::fromUTF8( boost::posix_time::to_simple_string( duration )), _table->elementAt(row, 1));

	Wt::WPushButton* btn = new Wt::WPushButton(Wt::WString::fromUTF8( "Play"), _table->elementAt(row, 2));
	btn->clicked().connect( boost::bind(&VideoDatabaseWidget::handlePlayVideo, this, path) );
}


void
VideoDatabaseWidget::updateView(Path::pointer directory)
{
	// Clear table from row 1 to end
//	while(_table->rowCount() > 2)
//		_table->deleteRow( _table->rowCount() - 1);

	_table->clear();

	addHeader();

	std::vector< Path::pointer > pathes;
	if(directory)
	{
		pathes = directory->getChilds();
		// Add parent directory
		addDirectory("<Parent>", directory->getParent() ? directory->getParent() ->getPath() : boost::filesystem::path());
	}
	else
		pathes = Path::getRoots(_db.getSession() );

	// Add childs
	BOOST_FOREACH(Path::pointer path, pathes)
	{
		std::cout << "Adding path " << path->getPath() << std::endl;
		if (path->isDirectory())
			addDirectory( path->getFileName(), path->getPath());
		else if (path->getVideo())
			addVideo( path->getFileName(), path->getVideo()->getDuration(), path->getPath());
		else
			std::cerr << "Bad type??" << std::endl;
	}

}

void
VideoDatabaseWidget::handleOpenDirectory(boost::filesystem::path path)
{
	Wt::Dbo::Transaction transaction ( _db.getSession() );

	Path::pointer directory = Path::getByPath(_db.getSession(), path);
	updateView(directory);
}

void
VideoDatabaseWidget::handlePlayVideo(const boost::filesystem::path& videoFilePath)
{
	std::cout << "Wants to play video " << videoFilePath << std::endl;

	Wt::Dbo::Transaction transaction ( _db.getSession() );

	Path::pointer path = Path::getByPath(_db.getSession(), videoFilePath);
	Video::pointer video;
	if (path)
		video = path->getVideo();

	if (!video)
	{
		std::cerr << "Cannot find video for this path '" << videoFilePath << "'" << std::endl;
		return;
	}

	// Emit signal
	playVideo().emit(videoFilePath);

}

} // namespace UserInterface

