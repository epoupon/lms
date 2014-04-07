#ifndef CODEC_HPP__
#define CODEC_HPP__

#include <string>
#include <memory>
#include <boost/utility.hpp>

#include "Common.hpp"

class Codec : boost::noncopyable
{
	friend class CodecContext;
	friend class FormatContext;
	friend class OutputFormatContext;

	public:

		typedef enum AVCodecID Id;
		enum Type {
			Encoder,
			Decoder,
		};

		Codec(Id codecId, Type type);
		Codec(const AVCodec* codec);		// Attach existing codec
		~Codec();

		Id		getId() const;
		std::string 	getName() const;

	private:

		const AVCodec* get() const;
		const AVCodec* _codec;
};

#endif
