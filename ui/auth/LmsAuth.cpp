
#include "LmsAuth.hpp"

namespace UserInterface {

LmsAuth::LmsAuth(Database::Handler& db)
: Wt::Auth::AuthWidget(db.getAuthService(),
		db.getUserDatabase(),
		db.getLogin())
{
}


void
LmsAuth::createLoggedInView()
{
	hide();
}

void
LmsAuth::createLoginView()
{
	show();
	Wt::Auth::AuthWidget::createLoginView();
}

} // namespace UserInterface
