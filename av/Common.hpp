#ifndef TRANSCODE_COMMON_HPP__
#define TRANSCODE_COMMON_HPP__

// Hack to properly iinclude libavcodec/avcodec.h...
//
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
# include <stdint.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
}

#include <string>


void AvInit();

class AvError
{
	public:
		AvError() : _errnum(0) {}
		AvError(int ernum) : _errnum(ernum) {}

		void operator=(int errnum) { _errnum = errnum; }

		operator bool() { return _errnum < 0; }

		std::string to_str() const;

		friend std::ostream& operator<<(std::ostream& ost, const AvError&);

		bool eof() { return _errnum == AVERROR_EOF; } // TODO

	private:
		int _errnum;
};


#endif
