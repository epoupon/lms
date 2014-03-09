#ifndef AV_DICTIONARY_HPP
#define AV_DICTIONARY_HPP

#include <map>
#include "Common.hpp"

namespace Av
{

class Dictionary
{
	public:
		Dictionary(AVDictionary* dictionary);

		// Get all
		void get(std::map<std::string, std::string>& entries);

		// get a single entry
		std::string	get(std::string key);

	private:
		AVDictionary*	_dictionary;
};

} // namespace Av

#endif

