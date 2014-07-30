#include <Wt/WRegExpValidator>

namespace UserInterface {

static inline Wt::WValidator *createEmailValidator()
{
	return new Wt::WRegExpValidator("[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,4}");
}

} // namespace UserInterface
