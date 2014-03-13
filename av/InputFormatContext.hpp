#ifndef INPUT_FORMAT_CONTEXT_HPP
#define INPUT_FORMAT_CONTEXT_HPP

#include <vector>

#include <boost/filesystem.hpp>

#include "FormatContext.hpp"
#include "Stream.hpp"
#include "Dictionary.hpp"

namespace Av
{

class InputFormatContext : public FormatContext
{
	public:

		InputFormatContext(const boost::filesystem::path& p);
		~InputFormatContext();


		Dictionary	getMetadata(void);	// metadata access

		// Scan file
		void findStreamInfo();


		// Get attached pictures
		void			getPictures(std::vector< std::vector<unsigned char> >& pictures) const;

		// Get the streams
		std::vector<Stream>	getStreams(void);

		bool 			getBestStreamIdx(enum AVMediaType type, Stream::Idx& idx);
		std::size_t		getDurationSecs() const;

	private:
		boost::filesystem::path _path;


};

} // namespace av

#endif

