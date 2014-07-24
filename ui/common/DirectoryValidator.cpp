#include <boost/filesystem.hpp>

#include "DirectoryValidator.hpp"

namespace UserInterface {

DirectoryValidator::DirectoryValidator(Wt::WObject *parent)
: Wt::WValidator(parent)
{
}


Wt::WValidator::Result
DirectoryValidator::validate(const Wt::WString& input) const
{
	if (input.empty())
		return Wt::WValidator::validate(input);

	boost::filesystem::path p(input.toUTF8());
	boost::system::error_code ec;

	bool res = boost::filesystem::is_directory(p, ec);
	if (ec)
		return Wt::WValidator::Result(Wt::WValidator::Invalid, ec.message());
	else if (res)
		return Wt::WValidator::Result(Wt::WValidator::Valid);
	else
		return Wt::WValidator::Result(Wt::WValidator::Invalid, "Not a directory");
}


} // namespace UserInterface
