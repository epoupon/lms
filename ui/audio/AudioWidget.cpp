#include <Wt/WBreak>

#include "AudioWidget.hpp"

#include "metadata/Extractor.hpp"

namespace UserInterface {

AudioWidget::AudioWidget(SessionData& sessionData, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
_sessionData(sessionData),
_audioDbWidget(nullptr),
_mediaPlayer(nullptr),
_imgResource(nullptr),
_img(nullptr)
{
	_audioDbWidget = new AudioDatabaseWidget(sessionData.getDatabaseHandler(), this);

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
			const std::vector< std::vector<unsigned char> >& pictures = inputFile.getCoverPictures();

			if (!pictures.empty())
			{
				std::cout << "Cover found!" << std::endl;
//				_imgResource->setMimeType(cover.mimeType);
				_imgResource->setData(pictures.front());
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

} // namespace UserInterface

