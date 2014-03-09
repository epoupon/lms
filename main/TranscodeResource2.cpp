
#include <fstream>

#include "TranscodeResource.hpp"

TranscodeResource::TranscodeResource(const boost::filesystem::path& p, const Transcoder::Parameters& params, Wt::WObject *parent)
: Wt::WResource(parent),
// _transcoder(params),
 _path(p)
{
//	suggestFileName(p.filename());
}

TranscodeResource::~TranscodeResource()
{
	beingDeleted();
}

void
TranscodeResource::handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response)
{
	std::ifstream r(_path.string().c_str(), std::ios::in | std::ios::binary);
	handleRequestPiecewise(request, response, r);
}

void
TranscodeResource::handleRequestPiecewise(const Wt::Http::Request& request,
                                             Wt::Http::Response& response,
                                             std::istream& input)
{
	suggestFileName( _path.filename().string() );
	Wt::Http::ResponseContinuation *continuation = request.continuation();
	::uint64_t startByte = continuation ?
		boost::any_cast< ::uint64_t >(continuation->data()) : 0;

	if (startByte == 0) {
		/*
		 * Initial request (not a continuation)
		 */
		if (!input) {
			response.setStatus(404);
			return;
		}

		/*
		 * See if we should return a range.
		 */
		input.seekg(0, std::ios::end);
		std::istream::pos_type isize = input.tellg();
		input.seekg(0, std::ios::beg);

		Wt::Http::Request::ByteRangeSpecifier ranges = request.getRanges(isize);

		if (!ranges.isSatisfiable()) {
			std::ostringstream contentRange;
			contentRange << "bytes */" << isize;
			response.setStatus(416); // Requested range not satisfiable
			response.addHeader("Content-Range", contentRange.str());
			return;
		}

		if (ranges.size() == 1) {
			response.setStatus(206);
			startByte = ranges[0].firstByte();
			_beyondLastByte = std::streamsize(ranges[0].lastByte() + 1);

			std::ostringstream contentRange;
			contentRange << "bytes " << startByte << "-"
				<< _beyondLastByte - 1 << "/" << isize;
			response.addHeader("Content-Range", contentRange.str());
			response.setContentLength(::uint64_t(_beyondLastByte) - startByte);
		} else {
			_beyondLastByte = std::streamsize(isize);
			response.setContentLength(::uint64_t(_beyondLastByte));
		}

		response.setMimeType("plain/text");
	}

	input.seekg(static_cast<std::istream::pos_type>(startByte));

	// According to 27.6.1.3, paragraph 1 of ISO/IEC 14882:2003(E),
	// each unformatted input function may throw an exception.
	boost::scoped_array<char> buf(new char[_bufferSize]);
	std::streamsize sbufferSize = std::streamsize(_bufferSize);

	std::streamsize restSize = _beyondLastByte - std::streamsize(startByte);
	std::streamsize pieceSize =  sbufferSize > restSize ? restSize : sbufferSize;

	std::cout << "Seeking to " << startByte << ", pieceSize = " << pieceSize << std::endl;

	input.read(buf.get(), pieceSize);
	std::streamsize actualPieceSize = input.gcount();
	response.out().write(buf.get(), actualPieceSize);

	if (input.good() && actualPieceSize < restSize) {
		continuation = response.createContinuation();
		continuation->setData(startByte + ::uint64_t(_bufferSize));
	}
}





