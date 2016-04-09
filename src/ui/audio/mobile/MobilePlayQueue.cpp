/*
 * Copyright (C) 2016 Emeric Poupon
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

#include <Wt/WTemplate>
#include <Wt/WImage>
#include <Wt/WText>
#include <Wt/WPushButton>

#include "logger/Logger.hpp"

#include "LmsApplication.hpp"

#include "MobilePlayQueue.hpp"


namespace UserInterface {
namespace Mobile {


PlayQueue::PlayQueue(Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent)
{
	Wt::WTemplate* t = new Wt::WTemplate(this);
	t->setTemplateText(Wt::WString::tr("wa-playqueue-view"));

	_trackContainer = new Wt::WContainerWidget();
	t->bindWidget("contents", _trackContainer);

	Wt::WTemplate* controls = new Wt::WTemplate();
	t->bindWidget("controls", controls);
	controls->setTemplateText(Wt::WString::tr("wa-playqueue-controls"));

	Wt::WText *loadBtn = new Wt::WText("<i class=\"fa fa-upload fa-2x\"></i>", Wt::XHTMLText);
	controls->bindWidget("load", loadBtn);
	loadBtn->setStyleClass("mobile-btn");

	Wt::WText *saveBtn = new Wt::WText("<i class=\"fa fa-download fa-2x\"></i>", Wt::XHTMLText);
	controls->bindWidget("save", saveBtn);
	saveBtn->setStyleClass("mobile-btn");

	Wt::WText *clearBtn = new Wt::WText("<i class=\"fa fa-remove fa-2x\"></i>", Wt::XHTMLText);
	controls->bindWidget("clear", clearBtn);
	clearBtn->setStyleClass("mobile-btn");
	clearBtn->clicked().connect(std::bind([=]
	{
		clear();
	}));

	_shuffleBtn = new Wt::WText("<i class=\"fa fa-random fa-2x\"></i>", Wt::XHTMLText);
	controls->bindWidget("shuffle", _shuffleBtn);
	_shuffleBtn->setStyleClass("mobile-btn");
	_shuffleBtn->clicked().connect(std::bind([=] ()
	{
		setShuffle(!_shuffle);
	}));

	_loopBtn = new Wt::WText("<i class=\"fa fa-repeat fa-2x\"></i>", Wt::XHTMLText);
	controls->bindWidget("repeat", _loopBtn);
	_loopBtn->setStyleClass("mobile-btn");
	_loopBtn->clicked().connect(std::bind([=] ()
	{
		setLoop(!_loop);
	}));

}

std::size_t
PlayQueue::addTrack(Database::Track::id_type id)
{
	Wt::Dbo::Transaction transaction(DboSession());

	Database::Track::pointer track = Database::Track::getById(DboSession(), id);
	if (!track)
	{
		LMS_LOG(UI, INFO) << "No track found for id " << id;
		return 0;
	}

	std::size_t trackPos = _trackIds.size();
	_trackIds.push_back(id);

	Wt::WTemplate* t = new Wt::WTemplate(_trackContainer);
	t->setTemplateText(Wt::WString::tr("wa-playqueue-track"));

	Wt::WImage *cover = new Wt::WImage();
	t->bindWidget("cover", cover);
	cover->setStyleClass ("center-block img-responsive");
	cover->setImageLink(SessionImageResource()->getTrackUrl(track.id(), 64));
	t->bindString("track-name", Wt::WString::fromUTF8(track->getName()), Wt::PlainText);
	t->bindString("artist-name", Wt::WString::fromUTF8(track->getArtist()->getName()), Wt::PlainText);

	Wt::WText *playBtn = new Wt::WText("<i class=\"fa fa-play fa-lg\"></i>", Wt::XHTMLText);
	t->bindWidget("play-btn", playBtn);
	playBtn->addStyleClass("mobile-btn");
	playBtn->clicked().connect(std::bind([=]
	{
		int pos = _trackContainer->indexOf(t);
		play(pos);
	}));

	Wt::WText *delBtn = new Wt::WText("<i class=\"fa fa-remove fa-lg\"></i>", Wt::XHTMLText);
	t->bindWidget("del-btn", delBtn);
	delBtn->addStyleClass("mobile-btn");
	delBtn->clicked().connect(std::bind([=]
	{
		int pos = _trackContainer->indexOf(t);
		_trackContainer->removeWidget(t);
		_trackIds.erase(_trackIds.begin() + pos);
	}));

	return trackPos;
}

void
PlayQueue::clear(void)
{
	_trackIds.clear();
	_trackContainer->clear();
	_currentPos = 0;
}

void
PlayQueue::play(size_t pos)
{
	if (_trackIds.empty() || pos >= _trackIds.size())
		return;

	_trackContainer->widget(_currentPos)->removeStyleClass("playqueue-playing");

	_currentPos = pos;
	_trackContainer->widget(_currentPos)->addStyleClass("playqueue-playing");

	_sigTrackPlay.emit(_trackIds[pos]);
}

void
PlayQueue::playNext(void)
{
	if (_currentPos == (_trackIds.size() - 1) && _loop)
		play(0);
	else
		play(_currentPos + 1);
}

void
PlayQueue::playPrevious(void)
{
	if (_currentPos == 0)
	{
		if (_loop)
		{
			play(_trackIds.size() - 1);
		}
		else
			return;
	}
	else
		play(_currentPos - 1);
}

void
PlayQueue::handlePlaybackComplete(void)
{
	playNext();
}

void
PlayQueue::setShuffle(bool value)
{
	_shuffle = value;

	if (value)
		_shuffleBtn->addStyleClass("mobile-btn-active");
	else
		_shuffleBtn->removeStyleClass("mobile-btn-active");
}

void
PlayQueue::setLoop(bool value)
{
	_loop = value;

	if (value)
		_loopBtn->addStyleClass("mobile-btn-active");
	else
		_loopBtn->removeStyleClass("mobile-btn-active");
}

} // namespace Mobile
} // namespace UserInterface
