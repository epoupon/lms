#ifndef FORMAT_CONTEXT_HPP
#define FORMAT_CONTEXT_HPP

#include <boost/filesystem.hpp>

#include "Common.hpp"

namespace Av
{

class FormatContext
{
	public:
		FormatContext();
		~FormatContext();


	protected:
		void		 native(AVFormatContext* c) { _context = c;}
		AVFormatContext* native() { return _context; }
		const AVFormatContext* native() const { return _context; }

	private:

		AVFormatContext* _context;


};

} // namespace Av

#endif

