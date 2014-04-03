
#include <Wt/WApplication>
#include <Wt/WLineEdit>

#include "audio/AudioWidget.hpp"
#include "video/VideoWidget.hpp"

class LmsApplication : public Wt::WApplication
{
	public:

		LmsApplication(const Wt::WEnvironment& env);

	private:

		void handleSearch();

		Wt::WLineEdit*	_searchEdit;
		AudioWidget*	_audioWidget;
		VideoWidget*	_videoWidget;

};

