/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "PulseAudioOutput.hpp"

#include <cassert>
#include <cstring>

#include "database/Track.hpp"
#include "utils/Logger.hpp"

std::unique_ptr<PulseAudioOutput>
createPulseAudioOutput(IAudioOutput::Format format, IAudioOutput::SampleRate sampleRate, std::size_t nbChannels)
{
	return std::make_unique<PulseAudioOutput>(format, sampleRate, nbChannels);
}

static
pa_sample_spec
constructSampleSpec(IAudioOutput::Format /*format*/, IAudioOutput::SampleRate sampleRate, std::size_t nbChannels)
{
	pa_sample_spec sampleSpec {};
	sampleSpec.format = PA_SAMPLE_S16LE; // TODO
	sampleSpec.rate = static_cast<uint32_t>(sampleRate);
	sampleSpec.channels = static_cast<uint8_t>(nbChannels);

	return sampleSpec;
}

PulseAudioOutput::PulseAudioOutput(Format format, SampleRate sampleRate, std::size_t nbChannels)
: _format {format}
, _sampleRate {sampleRate}
, _nbChannels {nbChannels}
, _sampleSpec {constructSampleSpec(format, sampleRate, nbChannels)}
{
	_mainLoop = MainLoopPtr{pa_mainloop_new()};
	if (!_mainLoop)
		throw PulseAudioException {"pa_mainloop_new failed!"};

	pa_mainloop_api *mainloopAPI {pa_mainloop_get_api(_mainLoop.get())};

	_context = ContextPtr {pa_context_new(mainloopAPI, "LMS")};
	if (!_context)
		throw PulseAudioException {"pa_context_new failed!"};

	pa_context_set_state_callback(_context.get(), [](pa_context*, void* userdata)
	{
		static_cast<PulseAudioOutput *>(userdata)->onContextStateChanged();
	}, this);

	start();

	LMS_LOG(LOCALPLAYER, INFO) << "Init done!";
}

PulseAudioOutput::~PulseAudioOutput()
{
	stop();
}

void
PulseAudioOutput::setOnCanWriteCallback(OnCanWriteCallback cb)
{
	_onCanWriteCallback = cb;
}

std::size_t
PulseAudioOutput::getCanWriteBytes()
{
	if (!_stream)
		return 0;

	return pa_stream_writable_size(_stream.get());
}

void
PulseAudioOutput::connect()
{
	LMS_LOG(LOCALPLAYER, INFO) << "Connecting to default server...";

	pa_context_flags_t flags {static_cast<pa_context_flags_t>(static_cast<unsigned>(PA_CONTEXT_NOFAIL) | static_cast<unsigned>(PA_CONTEXT_NOAUTOSPAWN))};

	if (pa_context_connect(_context.get(), nullptr, flags, nullptr) < 0)
		throw ContextPulseAudioException {_context.get(), "pa_context_connect failed!"};
}

void
PulseAudioOutput::disconnect()
{
	LMS_LOG(LOCALPLAYER, INFO) << "Disconnecting from server...";
	pa_context_disconnect(_context.get());
}

void
PulseAudioOutput::onContextStateChanged()
{
	switch (pa_context_get_state(_context.get()))
	{
		case PA_CONTEXT_UNCONNECTED:
			LMS_LOG(LOCALPLAYER, INFO) << "Unconnected from server";
			break;

		case PA_CONTEXT_CONNECTING: 
			LMS_LOG(LOCALPLAYER, INFO) << "Connecting to server...";
			break;

		case PA_CONTEXT_AUTHORIZING:
			LMS_LOG(LOCALPLAYER, INFO) << "Authorizing to server...";
			break;

		case PA_CONTEXT_SETTING_NAME:
			LMS_LOG(LOCALPLAYER, INFO) << "Setting name to server...";
			break;

		case PA_CONTEXT_READY:
			LMS_LOG(LOCALPLAYER, INFO) << "Connected to server '" << pa_context_get_server(_context.get()) << "'";
			createStream();
			connectStream();
			break;

		case PA_CONTEXT_FAILED:
			LMS_LOG(LOCALPLAYER, ERROR) << "Failed to connect to server";
			break;

		case PA_CONTEXT_TERMINATED:
			LMS_LOG(LOCALPLAYER, INFO) << "Connection closed";
			break;
	}

}

void
PulseAudioOutput::createStream()
{
	if (!pa_sample_spec_valid(&_sampleSpec))
		throw PulseAudioException {"Invalid sample specification!"};

	_stream = StreamPtr {pa_stream_new(_context.get(), "LMS-app", &_sampleSpec, nullptr)};
	if (!_stream)
		throw PulseAudioException {"pa_stream_new failed!"};

	pa_stream_set_state_callback(_stream.get(), [](pa_stream*, void* userdata)
	{
		static_cast<PulseAudioOutput *>(userdata)->onStreamStateChanged();
	}, this);

	pa_stream_set_write_callback(_stream.get(), [](pa_stream*, std::size_t nbytes, void *userdata)
	{
		static_cast<PulseAudioOutput *>(userdata)->onStreamCanWrite(nbytes);
	}, this);

	LMS_LOG(LOCALPLAYER, INFO) << "Stream created";
}


void
PulseAudioOutput::connectStream()
{
	pa_buffer_attr bufferAttr {};

	bufferAttr.maxlength	= static_cast<uint32_t>(-1);
	bufferAttr.tlength	= static_cast<uint32_t>(-1);
	bufferAttr.prebuf	= static_cast<uint32_t>(-1);
	bufferAttr.minreq	= static_cast<uint32_t>(-1);
	bufferAttr.fragsize	= static_cast<uint32_t>(-1);

	int err {pa_stream_connect_playback(_stream.get(),
				nullptr, // device
				&bufferAttr,
				PA_STREAM_NOFLAGS, // flags
				nullptr, // volume 
				nullptr // standalone stream
				)};
	if (err != 0)
		throw PulseAudioException {err, "pa_stream_connect_playback failed!"};
}

void
PulseAudioOutput::onStreamStateChanged()
{
	switch (pa_stream_get_state(_stream.get()))
	{
		case PA_STREAM_UNCONNECTED:
			LMS_LOG(LOCALPLAYER, DEBUG) << "Stream is unconnected";
			break;

		case PA_STREAM_CREATING:
			LMS_LOG(LOCALPLAYER, DEBUG) << "Stream is creating...";
			break;

		case PA_STREAM_READY:
		{
			const auto* formatInfo {pa_stream_get_format_info(_stream.get())};
			LMS_LOG(LOCALPLAYER, DEBUG) << "Encoding = " << pa_encoding_to_string(formatInfo->encoding);
			LMS_LOG(LOCALPLAYER, DEBUG) << "props = " << pa_proplist_to_string(formatInfo->plist);
			LMS_LOG(LOCALPLAYER, DEBUG) << "dev = " << pa_stream_get_device_name(_stream.get());
			LMS_LOG(LOCALPLAYER, DEBUG) << " Stream is ready";
		}
			break;

		case PA_STREAM_FAILED:
			LMS_LOG(LOCALPLAYER, ERROR) << "Stream failed!";
			break;

		case PA_STREAM_TERMINATED:
			LMS_LOG(LOCALPLAYER, INFO) << "Stream terminated";
			break;

	}
}

static
std::size_t
getAlignedFrameSize(std::size_t l, const pa_sample_spec& sampleSpec)
{
     const size_t fs {pa_frame_size(&sampleSpec)};
     return (l/fs) * fs;
}

void
PulseAudioOutput ::onStreamCanWrite(std::size_t maxBytesCount)
{
	LMS_LOG(LOCALPLAYER, DEBUG) << "Stream: can write up to " << maxBytesCount << " bytes";	

	if (_onCanWriteCallback)
		_onCanWriteCallback(maxBytesCount);
}

std::size_t
PulseAudioOutput::write(const unsigned char* data, std::size_t size)
{
	const std::size_t writtenBytes {getAlignedFrameSize(std::min(size, getCanWriteBytes()), _sampleSpec)};

	if (writtenBytes == 0)
		return 0;

	int res {pa_stream_write(_stream.get(),
				data, writtenBytes, nullptr /* internal copy */,
				0, // offset
				PA_SEEK_RELATIVE)};
	if (res != 0)
		throw PulseAudioException {res, "pa_stream_write failed"};

	return writtenBytes;
}

void
PulseAudioOutput::start()
{
	LMS_LOG(LOCALPLAYER, INFO) << "Starting PA output...";
	assert(!_thread);

	connect();

	_thread = std::make_unique<std::thread>([&]() { threadEntry(); } );
	LMS_LOG(LOCALPLAYER, INFO) << "Started PA output!";
}

void
PulseAudioOutput::stop()
{
	LMS_LOG(LOCALPLAYER, INFO) << "Stopping PA output...";
	assert(_thread);

	disconnect();

	pa_mainloop_quit(_mainLoop.get(), 1);

	_thread->join();
	LMS_LOG(LOCALPLAYER, INFO) << "Stopped PA output!";
}

void
PulseAudioOutput::threadEntry()
{
	LMS_LOG(LOCALPLAYER, INFO) << "Running...";

	int stopRequested {};

	try
	{
		pa_mainloop_run(_mainLoop.get(),  &stopRequested);
	}
	catch (const PulseAudioException& e)
	{
		LMS_LOG(LOCALPLAYER, ERROR) << "Caught exception: " << e.what();
	}

	if (stopRequested)
		LMS_LOG(LOCALPLAYER, INFO) << "Stopped!";
	else
		LMS_LOG(LOCALPLAYER, ERROR) << "Mainloop error!";
}


