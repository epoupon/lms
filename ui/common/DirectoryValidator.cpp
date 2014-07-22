#include <boost/filesystem.hpp>

#include "DirectoryValidator.hpp"

namespace UserInterface {

DirectoryValidator::DirectoryValidator(bool mandatory, Wt::WObject *parent)
: Wt::WValidator(mandatory, parent)
{
}


Wt::WValidator::Result
DirectoryValidator::validate(const Wt::WString& input) const
{
	boost::filesystem::path p(input.toUTF8());
	boost::system::error_code ec;

	bool res = boost::filesystem::is_directory(p, ec);
	if (ec)
		return Wt::WValidator::Result(Wt::WValidator::Invalid, ec.message());
	else if (res)
		return Wt::WValidator::Result(Wt::WValidator::Valid, "Valid");
	else
		return Wt::WValidator::Result(Wt::WValidator::Invalid, "Not a directory");
}


} // namespace UserInterface
