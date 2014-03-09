#ifndef VIDEO_WIDGET_HPP
#define VIDEO_WIDGET_HPP

#include <string>

#include <Wt/WContainerWidget>

#include "video/VideoMediaPlayerWidget.hpp"
#include "video/VideoDatabaseWidget.hpp"

class VideoWidget : public Wt::WContainerWidget
{
	public:

		VideoWidget(Wt::WContainerWidget* parent = 0);

		void search(const std::string& searchText);

	private:

		void backToList(void);
		void playVideo(boost::filesystem::path p);

		VideoDatabaseWidget*	_videoDbWidget;
		VideoMediaPlayerWidget*	_mediaPlayer;

};

#endif

