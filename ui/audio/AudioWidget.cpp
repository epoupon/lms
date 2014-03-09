#include <Wt/WBreak>

#include "AudioWidget.hpp"

#include "metadata/Extractor.hpp"


AudioWidget::AudioWidget(Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
_audioDbWidget(nullptr),
_mediaPlayer(nullptr),
_imgResource(nullptr),
_img(nullptr)
{
	_audioDbWidget = new AudioDatabaseWidget(this);

	_audioDbWidget->trackSelected().connect(this, &AudioWidget::playTrack);

	_mediaPlayer = new AudioMediaPlayerWidget(this);
	_mediaPlayer->playbackEnded().connect(this, &AudioWidget::handleTrackEnded);
	this->addWidget(new Wt::WBreak());

	// Image
	_imgResource = new Wt::WMemoryResource(this);
	_imgLink.setResource( _imgResource);
	_img = new Wt::WImage(_imgLink, this);

}

void
AudioWidget::search(const std::string& searchText)
{
	_audioDbWidget->search(searchText);
}


void
AudioWidget::playTrack(boost::filesystem::path p)
{
	std::cout << "play track '" << p << "'" << std::endl;
	try {
		// TODO get user's encoding preference

		Transcode::InputMediaFile inputFile(p);

		Transcode::Parameters parameters(inputFile, Transcode::Format::get(Transcode::Format::OGA), 128000);

		_mediaPlayer->load( parameters );

		// Refresh cover
		{
			MetaData::Extractor parser;
			MetaData::GenericData cover;
			if (parser.parseCover( p, cover)) {
				std::cout << "Cover parsed!" << std::endl;
				std::cout << "mime type = " << cover.mimeType << ", size = " << cover.data.size();

				_imgResource->setMimeType(cover.mimeType);
				_imgResource->setData(cover.data);

			}
			else {
				std::cout << "No cover found!" << std::endl;
				_imgResource->setData( std::vector<unsigned char>());
			}

			_imgResource->setChanged();
		}
	}
	catch( std::exception &e)
	{
		std::cerr <<"Caught exception while loading '" << p << "': " << e.what() << std::endl;
	}
}

	void
AudioWidget::handleTrackEnded(void)
{
	std::cout << "Track playback ended!" << std::endl;
	_audioDbWidget->selectNextTrack();
}


