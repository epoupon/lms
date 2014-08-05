#include <Wt/WBreak>

#include "AudioWidget.hpp"

namespace UserInterface {

AudioWidget::AudioWidget(SessionData& sessionData, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
_db(sessionData.getDatabaseHandler()),
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

		std::size_t bitrate = 0;

		// Get user preferences
		{
			Wt::Dbo::Transaction transaction(_db.getSession());
			Database::User::pointer user = _db.getCurrentUser();
			if (user)
				bitrate = user->getAudioBitrate();
			else
				return; // TODO logout?
		}

		Transcode::InputMediaFile inputFile(p);

		// TODO get the input stream bitrate and min the result with the desired bitrate
		Transcode::Parameters parameters(inputFile, Transcode::Format::get(Transcode::Format::OGA), bitrate);

		_mediaPlayer->load( parameters );

		// Refresh cover
		{
			std::vector<CoverArt::CoverArt> covers = inputFile.getCovers();

			if (!covers.empty())
			{
				std::cout << "Cover found!" << std::endl;
				if (!covers.front().scale(256))
					std::cerr << "Cannot resize!" << std::endl;

				//_imgResource->setMimeType(covers.front().getMimeType());
				_imgResource->setData(covers.front().getData());
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

