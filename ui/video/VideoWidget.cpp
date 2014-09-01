#include <Wt/WApplication>
#include <Wt/WEnvironment>

#include "VideoWidget.hpp"

namespace UserInterface {

VideoWidget::VideoWidget(SessionData& sessionData, Wt::WContainerWidget* parent )
: Wt::WContainerWidget(parent),
_sessionData(sessionData)
{

	_videoDbWidget = new VideoDatabaseWidget(_sessionData.getDatabaseHandler(), this);

	_videoDbWidget->playVideo().connect(this, &VideoWidget::playVideo);


}


void
VideoWidget::search(const std::string& searchText)
{
	//TODO
}

void
VideoWidget::backToList(void)
{
	if (_mediaPlayer)
		delete _mediaPlayer;

	_videoDbWidget->setHidden(false);

}


void
VideoWidget::playVideo(boost::filesystem::path p)
{
	std::cout << "Want to play video " << p << "'" << std::endl;
	try {

		// TODO get user's encoding preference
		Transcode::InputMediaFile inputFile(p);

		Transcode::Format::Encoding encoding;

		if (Wt::WApplication::instance()->environment().agentIsChrome())
			encoding = Transcode::Format::WEBMV;
		else
			encoding = Transcode::Format::FLV;

		Transcode::Parameters parameters(inputFile, Transcode::Format::get(encoding));

		parameters.setBitrate(Transcode::Stream::Audio, 128000); // TODO
		parameters.setBitrate(Transcode::Stream::Video, 500000); // TODO

		_mediaPlayer = new VideoMediaPlayerWidget(parameters, this);
		_mediaPlayer->close().connect(this, &VideoWidget::backToList);

		_videoDbWidget->setHidden(true);
	}
	catch( std::exception& e) {
		std::cerr <<"Caught exception while loading '" << p << "': " << e.what() << std::endl;
	}
}

} // namespace UserInterface

