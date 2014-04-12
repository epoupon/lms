#ifndef VIDEO_WIDGET_HPP
#define VIDEO_WIDGET_HPP

#include <string>

#include <Wt/WContainerWidget>

#include "common/SessionData.hpp"

#include "video/VideoMediaPlayerWidget.hpp"
#include "video/VideoDatabaseWidget.hpp"

namespace UserInterface {

class VideoWidget : public Wt::WContainerWidget
{
	public:

		VideoWidget(SessionData& sessionData, Wt::WContainerWidget* parent = 0);

		void search(const std::string& searchText);

	private:

		void backToList(void);
		void playVideo(boost::filesystem::path p);

		SessionData&		_sessionData;

		VideoDatabaseWidget*	_videoDbWidget;
		VideoMediaPlayerWidget*	_mediaPlayer;

};

} // namespace UserInterface

#endif

