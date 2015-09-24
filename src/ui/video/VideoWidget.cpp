/*
 * Copyright (C) 2013 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Wt/WApplication>
#include <Wt/WEnvironment>

#include "logger/Logger.hpp"

#include "LmsApplication.hpp"

#include "VideoWidget.hpp"

namespace UserInterface {

VideoWidget::VideoWidget(Wt::WContainerWidget* parent )
: Wt::WContainerWidget(parent)
{

	_videoDbWidget = new VideoDatabaseWidget(this);

	_videoDbWidget->playVideo().connect(this, &VideoWidget::playVideo);


}


void
VideoWidget::search(const std::string& searchText)
{
	//TODO
}

void
VideoWidget::playVideo(boost::filesystem::path p)
{
	LMS_LOG(UI, DEBUG) << "Want to play video " << p << "'";

	std::size_t audioBitrate = 0;
	std::size_t videoBitrate = 0;

	// Get user preferences
	{
		Wt::Dbo::Transaction transaction(DboSession());

		audioBitrate = CurrentUser()->getMaxAudioBitrate();
		videoBitrate = CurrentUser()->getMaxVideoBitrate();
	}

	LMS_LOG(UI, DEBUG) << "Max bitrate set to " << videoBitrate << "/" << audioBitrate;

	Av::MediaFile mediaFile(p);

	if (!mediaFile.open())
		return;

	if (!mediaFile.scan())
		return;

	Av::TranscodeParameters parameters;

	parameters.setEncoding( Av::Encoding::WEBMV );

	VideoMediaPlayerWidget *mediaPlayer = new VideoMediaPlayerWidget(mediaFile, parameters, this);

	mediaPlayer->close().connect(std::bind([=] ()
				{
				_videoDbWidget->setHidden(false);
				delete mediaPlayer;
				}));

	_videoDbWidget->setHidden(true);
}

} // namespace UserInterface

