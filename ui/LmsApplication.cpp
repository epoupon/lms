#include <Wt/WBootstrapTheme>
#include "auth/LmsAuth.hpp"

#include "LmsHome.hpp"
#include "settings/SettingsFirstConnectionFormView.hpp"

#include "LmsApplication.hpp"

namespace skeletons {
	  extern const char *AuthStrings_xml1;
}


namespace UserInterface {

Wt::WApplication*
LmsApplication::create(const Wt::WEnvironment& env, boost::filesystem::path dbPath)
{
	/*
	 * You could read information from the environment to decide whether
	 * the user has permission to start a new application
	 */
	return new LmsApplication(env, dbPath);
}

/*
 * The env argument contains information about the new session, and
 * the initial request. It must be passed to the Wt::WApplication
 * constructor so it is typically also an argument for your custom
 * application constructor.
*/
LmsApplication::LmsApplication(const Wt::WEnvironment& env, boost::filesystem::path dbPath)
: Wt::WApplication(env),
 _sessionData(dbPath),
 _home(nullptr)
{

	Wt::WBootstrapTheme *bootstrapTheme = new Wt::WBootstrapTheme(this);
	bootstrapTheme->setVersion(Wt::WBootstrapTheme::Version3);
	bootstrapTheme->setResponsive(true);
	setTheme(bootstrapTheme);

	// Add a resource bundle
	messageResourceBundle().use(appRoot() + "templates");

	setTitle("LMS");                               // application title

	bool firstConnection;
	{
		Wt::Dbo::Transaction transaction(_sessionData.getDatabaseHandler().getSession());

		firstConnection = (Database::User::getAll(_sessionData.getDatabaseHandler().getSession()).size() == 0);
	}

	// If here is no account in the database, launch the first connection wizard
	if (firstConnection)
	{
		// Hack, use the auth widget builtin strings
		builtinLocalizedStrings().useBuiltin(skeletons::AuthStrings_xml1);

		root()->addWidget( new Settings::FirstConnectionFormView(_sessionData));
	}
	else
	{
		_sessionData.getDatabaseHandler().getLogin().changed().connect(this, &LmsApplication::handleAuthEvent);

		LmsAuth *authWidget = new LmsAuth(_sessionData.getDatabaseHandler());

		authWidget->model()->addPasswordAuth(&Database::Handler::getPasswordService());
		authWidget->setRegistrationEnabled(false);

		authWidget->processEnvironment();

		root()->addWidget(authWidget);
	}
}



void
LmsApplication::handleAuthEvent(void)
{
	if (_sessionData.getDatabaseHandler().getLogin().loggedIn())
	{
		if (_home == nullptr) {
			_home = new LmsHome(_sessionData, root() );
		}
		else
			std::cerr << "Already logged in??" << std::endl;
	}
	else
	{
		std::cerr << "user log out" << std::endl;
		if (_home != nullptr) {
			delete _home;
			_home = nullptr;

			// Hack: quit/redirect in order to avoid 'signal not exposed' problems
			// TODO, investigate/remove?
			quit();
			redirect("/");

		}
		else
			std::cerr << "Already logged out??" << std::endl;
	}
}

} // namespace UserInterface


