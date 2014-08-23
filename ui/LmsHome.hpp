#ifndef LMS_HOME_HPP
#define LMS_HOME_HPP

#include <Wt/WContainerWidget>
#include <Wt/WLineEdit>

#include "common/SessionData.hpp"

#include "audio/AudioWidget.hpp"
#include "video/VideoWidget.hpp"

namespace UserInterface {

class LmsHome : public Wt::WContainerWidget
{
	public:

		LmsHome(SessionData& sessionData, Wt::WContainerWidget* parent = 0);

	private:

		void handleSearch(void);
		void handleUserMenuSelected( Wt::WMenuItem* item );

		SessionData&	_sessionData;

		Wt::WLineEdit*	_searchEdit;
		AudioWidget*	_audioWidget;
		VideoWidget*	_videoWidget;
};

} // namespace UserInterface

#endif

