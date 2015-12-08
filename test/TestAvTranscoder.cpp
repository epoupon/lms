#include <stdlib.h>

#include <stdexcept>
#include <iostream>

#include "av/AvInfo.hpp"
#include "av/AvTranscoder.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cerr << "Usage: <file>" << std::endl;
		return EXIT_FAILURE;
	}

	try
	{
		Av::AvInit();
		Av::Transcoder::init();

		// Make pstream work with ffmpeg
		close(STDIN_FILENO);

		Av::TranscodeParameters parameters;
		parameters.setEncoding(Av::Encoding::MP3);
		parameters.setOffset( boost::posix_time::seconds(0) );
		parameters.setBitrate( Av::Stream::Type::Audio, 160000 );
//		parameters.addStream(0);

		Av::Transcoder transcoder(argv[1], parameters);

		if (!transcoder.start())
			throw std::runtime_error("transcoder.start failed!");

		while (!transcoder.isComplete())
		{
			std::vector<unsigned char> data;
			std::cout << "Processing ..." << std::endl;
			transcoder.process(data, 65536);
			std::cout << "Processing done" << std::endl;
		}

		std::cout << "Complete!" << std::endl;

		return EXIT_SUCCESS;
	}
	catch (std::exception& e)
	{
		std::cerr << "Caught exception: " << e.what();
		return EXIT_FAILURE;
	}

}

