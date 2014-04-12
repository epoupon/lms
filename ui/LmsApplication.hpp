
#include <Wt/WApplication>
#include <Wt/WLineEdit>

#include "audio/AudioWidget.hpp"
#include "video/VideoWidget.hpp"

namespace UserInterface {


class LmsApplication : public Wt::WApplication
{
	public:

		static Wt::WApplication *create(const Wt::WEnvironment& env, boost::filesystem::path dbPath);

		LmsApplication(const Wt::WEnvironment& env, boost::filesystem::path dbPath);

	private:

		void handleSearch();

		SessionData	_sessionData;

		Wt::WLineEdit*	_searchEdit;
		AudioWidget*	_audioWidget;
		VideoWidget*	_videoWidget;

};


} // namespace UserInterface
