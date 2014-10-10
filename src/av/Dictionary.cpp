/*
 * Copyright (C) 2013 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

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

