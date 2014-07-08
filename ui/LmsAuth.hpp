#include <Wt/Auth/AuthService>
#include <Wt/Auth/AbstractUserDatabase>
#include <Wt/Auth/Login>
#include <Wt/Auth/AuthWidget>
#include <Wt/WContainerWidget>

namespace UserInterface {

class LmsAuth : public Wt::Auth::AuthWidget
{
	public:

		LmsAuth(const Wt::Auth::AuthService &baseAuth,
				Wt::Auth::AbstractUserDatabase &users,
				Wt::Auth::Login &login,
				Wt::WContainerWidget *parent=0)
			: Wt::Auth::AuthWidget(baseAuth, users, login, parent) {}

		// LoggedInView is delegated to LmsHome
		void createLoggedInView () { hide();}

		void createLoginView () { show(); Wt::Auth::AuthWidget::createLoginView();}

	private:


};

} // namespace UserInterface
