#ifndef EXTRACTOR_HPP
#define EXTRACTOR_HPP

#include <extractor.h>

#include "MetaData.hpp"


namespace MetaData
{

// Implements GNU libextractor library
class Extractor : public Parser
{
	public:

		Extractor();
		~Extractor();

		void parse(const boost::filesystem::path& p, Items& items);

		bool parseCover(const boost::filesystem::path& p, GenericData& data);

	private:

		struct EXTRACTOR_PluginList *_plugins;
};

} // namespace MetaData

#endif

