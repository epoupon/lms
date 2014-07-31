#include <Wt/WRegExpValidator>
#include <Wt/WLengthValidator>

#include "database/User.hpp"

namespace UserInterface {

static inline Wt::WValidator *createEmailValidator()
{
	Wt::WValidator *res = new Wt::WRegExpValidator("[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,4}");
	res->setMandatory(true);
	return res;
}

static inline Wt::WValidator *createNameValidator() {
	Wt::WLengthValidator *v = new Wt::WLengthValidator();
	v->setMandatory(true);
	v->setMinimumLength(3);
	v->setMaximumLength(::Database::User::MaxNameLength);
	return v;
}

} // namespace UserInterface
