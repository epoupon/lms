#include "Dictionary.hpp"


namespace Av
{

Dictionary::Dictionary(AVDictionary* dictionary)
: _dictionary(dictionary)
{
}

void
Dictionary::get(std::map<std::string, std::string>& entries)
{
	AVDictionaryEntry *tag = NULL;
	while ((tag = av_dict_get(_dictionary, "", tag, AV_DICT_IGNORE_SUFFIX))) {
		entries.insert( std::make_pair(tag->key, tag->value));
	}
}

std::string
Dictionary::get(std::string key)
{
	AVDictionaryEntry *tag = NULL;
	tag = av_dict_get(_dictionary, key.c_str(), tag, 0);

	return tag != NULL ? std::string(tag->value) : std::string();
}

} // namespace Av

