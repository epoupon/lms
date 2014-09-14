#include <Wt/WBreak>

#include "logger/Logger.hpp"

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
	LMS_LOG(MOD_UI, SEV_DEBUG) << "play track '" << p << "'";
	try {

		std::size_t bitrate = 0;

		// Get user preferences
		{
			Wt::Dbo::Transaction transaction(_db.getSession());
			Database::User::pointer user = _db.getCurrentUser();
			if (user)
				bitrate = user->getAudioBitrate();
			else
			{
				LMS_LOG(MOD_UI, SEV_ERROR) << "Can't play video: user does not exists!";
				return; // TODO logout?
			}
		}

		Transcode::InputMediaFile inputFile(p);

		Transcode::Parameters parameters(inputFile, Transcode::Format::get(Transcode::Format::OGA));

		parameters.setBitrate(Transcode::Stream::Audio, bitrate);

		_mediaPlayer->load( parameters );

		// Refresh cover
		{
			std::vector<CoverArt::CoverArt> covers = inputFile.getCovers();

			if (!covers.empty())
			{
				LMS_LOG(MOD_UI, SEV_DEBUG) << "Cover found!";
				if (!covers.front().scale(256)) // TODO
					LMS_LOG(MOD_UI, SEV_ERROR) << "Cannot resize!";

				//_imgResource->setMimeType(covers.front().getMimeType());
				_imgResource->setData(covers.front().getData());
			}
			else {
				LMS_LOG(MOD_UI, SEV_DEBUG) << "No cover found!";
				_imgResource->setData( std::vector<unsigned char>());
			}

			_imgResource->setChanged();
		}
	}
	catch( std::exception &e)
	{
		LMS_LOG(MOD_UI, SEV_ERROR) << "Caught exception while loading '" << p << "': " << e.what();
	}
}

	void
AudioWidget::handleTrackEnded(void)
{
	LMS_LOG(MOD_UI, SEV_DEBUG) << "Track playback ended!";
	_audioDbWidget->selectNextTrack();
}

} // namespace UserInterface

