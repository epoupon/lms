#ifndef LMS_APPLICATION_HPP
#define LMS_APPLICATION_HPP

#include <Wt/WApplication>

#include "common/SessionData.hpp"

#include "LmsHome.hpp"

namespace UserInterface {


class LmsApplication : public Wt::WApplication
{
	public:

		static Wt::WApplication *create(const Wt::WEnvironment& env, boost::filesystem::path dbPath);

		LmsApplication(const Wt::WEnvironment& env, boost::filesystem::path dbPath);

	private:

		void handleAuthEvent(void);

		SessionData	_sessionData;

		LmsHome*	_home;

};


} // namespace UserInterface

#endif

