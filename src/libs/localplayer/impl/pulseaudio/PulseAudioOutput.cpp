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
#include <iomanip>
#include <thread>

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
	_mainLoop = MainLoopPtr {pa_threaded_mainloop_new()};
	if (!_mainLoop)
		throw PulseAudioException {"pa_mainloop_new failed!"};

	init();

	LMS_LOG(PA, INFO) << "Init done!";
}

PulseAudioOutput::~PulseAudioOutput()
{
	deinit();
}

void
PulseAudioOutput::start()
{
	MainLoopLock lock {_mainLoop};

	createStream();
	connectStream();



}

void
PulseAudioOutput::stop()
{
	MainLoopLock lock {_mainLoop};

	disconnectStream();
	destroyStream();
}

void
PulseAudioOutput::flush()
{
	LMS_LOG(PA, DEBUG) << "Flushing stream...";

	LMS_LOG(LOCALPLAYER, DEBUG) << "WRITE @ " << std::fixed << std::setprecision(3) << getCurrentWriteTime().count() / float {1000};

	pa_operation *operation {};
	{
		MainLoopLock lock {_mainLoop};

		if (!_stream)
			return;

		operation = pa_stream_flush(_stream.get(), NULL, NULL);
	}

	// TODO: something better here
	while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
	{
		LMS_LOG(PA, DEBUG) << "Still running...!";
		std::this_thread::yield();
	}

	LMS_LOG(LOCALPLAYER, DEBUG) << "WRITE @ " << std::fixed << std::setprecision(3) << getCurrentWriteTime().count() / float {1000};
	LMS_LOG(PA, DEBUG) << "Flushed stream!";
}

void
PulseAudioOutput::setOnCanWriteCallback(OnCanWriteCallback cb)
{
	MainLoopLock lock {_mainLoop};

	_onCanWriteCallback = cb;
}

std::size_t
PulseAudioOutput::getCanWriteBytes()
{
	MainLoopLock lock {_mainLoop};

	if (!_stream)
		return 0;

	return pa_stream_writable_size(_stream.get());
}

void
PulseAudioOutput::createContext()
{
	LMS_LOG(PA, INFO) << "Connecting to default server...";

	MainLoopLock lock {_mainLoop};

	pa_mainloop_api *mainloopAPI {pa_threaded_mainloop_get_api(_mainLoop.get())};
	_context = ContextPtr {pa_context_new(mainloopAPI, "LMS")};
	if (!_context)
		throw PulseAudioException {"pa_context_new failed!"};

	pa_context_set_state_callback(_context.get(), [](pa_context*, void* userdata)
	{
		static_cast<PulseAudioOutput *>(userdata)->onContextStateChanged();
	}, this);

	pa_context_flags_t flags {static_cast<pa_context_flags_t>(static_cast<unsigned>(PA_CONTEXT_NOFAIL) | static_cast<unsigned>(PA_CONTEXT_NOAUTOSPAWN))};
	if (pa_context_connect(_context.get(), nullptr, flags, nullptr) < 0)
		throw ContextPulseAudioException {_context.get(), "pa_context_connect failed!"};
}

void
PulseAudioOutput::destroyContext()
{
	LMS_LOG(PA, INFO) << "Disconnecting from server...";

	MainLoopLock lock {_mainLoop};

	pa_context_disconnect(_context.get());
	_context.reset();
}

void
PulseAudioOutput::onContextStateChanged()
{
	switch (pa_context_get_state(_context.get()))
	{
		case PA_CONTEXT_UNCONNECTED:
			LMS_LOG(PA, INFO) << "Unconnected from server";
			break;

		case PA_CONTEXT_CONNECTING: 
			LMS_LOG(PA, INFO) << "Connecting to server...";
			break;

		case PA_CONTEXT_AUTHORIZING:
			LMS_LOG(PA, INFO) << "Authorizing to server...";
			break;

		case PA_CONTEXT_SETTING_NAME:
			LMS_LOG(PA, INFO) << "Setting name to server...";
			break;

		case PA_CONTEXT_READY:
			LMS_LOG(PA, INFO) << "Connected to server '" << pa_context_get_server(_context.get()) << "'";
			break;

		case PA_CONTEXT_FAILED:
			LMS_LOG(PA, ERROR) << "Failed to connect to server";
			break;

		case PA_CONTEXT_TERMINATED:
			LMS_LOG(PA, INFO) << "Connection closed";
			break;
	}

}

void
PulseAudioOutput::createStream()
{
	assert(!_stream);

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

	LMS_LOG(PA, INFO) << "Stream created";
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
				static_cast<pa_stream_flags_t>(PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE),
				nullptr, // volume 
				nullptr // standalone stream
				)};
	if (err != 0)
		throw PulseAudioException {err, "pa_stream_connect_playback failed!"};
}

void
PulseAudioOutput::disconnectStream()
{
	LMS_LOG(PA, INFO) << "Disconnecting stream...";

	if (!_stream)
		return;

	if (pa_stream_get_state(_stream.get()) == PA_STREAM_READY)
	{
		int err {pa_stream_disconnect(_stream.get())};
		if (err != 0)
			throw PulseAudioException {err, "pa_stream_disconnect failed!"};

		LMS_LOG(PA, INFO) << "Stream disconnected!";
	}
}

void
PulseAudioOutput::onStreamStateChanged()
{
	if (!_stream)
		return;

	switch (pa_stream_get_state(_stream.get()))
	{
		case PA_STREAM_UNCONNECTED:
			LMS_LOG(PA, DEBUG) << "Stream state: unconnected";
			break;

		case PA_STREAM_CREATING:
			LMS_LOG(PA, DEBUG) << "Stream state: creating...";
			break;

		case PA_STREAM_READY:
		{
			const auto* formatInfo {pa_stream_get_format_info(_stream.get())};
			LMS_LOG(PA, DEBUG) << "Stream state: ready";
			LMS_LOG(PA, DEBUG) << " Encoding = " << pa_encoding_to_string(formatInfo->encoding);
			LMS_LOG(PA, DEBUG) << " props = " << pa_proplist_to_string(formatInfo->plist);
			LMS_LOG(PA, DEBUG) << " dev = " << pa_stream_get_device_name(_stream.get());

			//TODO: check format selected by server?
		}
			break;

		case PA_STREAM_FAILED:
			LMS_LOG(PA, DEBUG) << "Stream state: failed!";
			break;

		case PA_STREAM_TERMINATED:
			LMS_LOG(PA, DEBUG) << "Stream state: terminated!";
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
PulseAudioOutput::onStreamCanWrite(std::size_t maxBytesCount)
{
	LMS_LOG(PA, DEBUG) << "Stream: can write up to " << maxBytesCount << " bytes";	
	if (!_stream)
	{
		LMS_LOG(PA, DEBUG) << "No stream, not notifying CanWriteCallback";
		return;
	}


	if (_onCanWriteCallback)
		_onCanWriteCallback(maxBytesCount);
}

void
PulseAudioOutput::destroyStream()
{
	LMS_LOG(PA, DEBUG) << "Destroying stream...";
	_stream.reset();
	LMS_LOG(PA, DEBUG) << "Destroyed stream!";
}

std::size_t
PulseAudioOutput::write(const unsigned char* data, std::size_t size, std::optional<std::chrono::milliseconds> writeTime)
{
	MainLoopLock lock {_mainLoop};

	LMS_LOG(PA, DEBUG) << "Want to write " << size << " bytes";
	if (writeTime)
		LMS_LOG(PA, DEBUG) << "\t@ " << std::fixed << std::setprecision(3) << writeTime->count() / float {1000};

	const std::size_t writtenBytes {getAlignedFrameSize(std::min(size, getCanWriteBytes()), _sampleSpec)};
	if (writtenBytes > 0)
	{
		int res {pa_stream_write(_stream.get(),
				data, writtenBytes, nullptr /* internal copy */,
				writeTime ? pa_usec_to_bytes(std::chrono::duration_cast<std::chrono::microseconds>(*writeTime).count(), &_sampleSpec) : 0, // ofset
				writeTime ? PA_SEEK_ABSOLUTE : PA_SEEK_RELATIVE)};
		if (res != 0)
			throw PulseAudioException {res, "pa_stream_write failed"};
	}

	LMS_LOG(PA, INFO) << "Written " << writtenBytes << " bytes!";

	return writtenBytes;
}

void
PulseAudioOutput::init()
{
	LMS_LOG(PA, INFO) << "Initializing PA output...";

	pa_threaded_mainloop_start(_mainLoop.get());
	createContext();

	LMS_LOG(PA, INFO) << "Initialized PA output!";
}

void
PulseAudioOutput::deinit()
{
	LMS_LOG(PA, INFO) << "Deinitializing PA output...";

	destroyContext();
	pa_threaded_mainloop_stop(_mainLoop.get());

	LMS_LOG(PA, INFO) << "Deinitialized PA output...";
}

std::chrono::milliseconds
PulseAudioOutput::getCurrentReadTime()
{
	{
		pa_operation *operation {};
		{
			MainLoopLock lock {_mainLoop};

			if (!_stream)
				return {};
	if (pa_stream_get_state(_stream.get()) != PA_STREAM_READY)
	{
		LMS_LOG(PA, DEBUG) << "Stream not ready yet, skip get_time";
		return {};
	}

			operation = pa_stream_update_timing_info(_stream.get(), NULL, NULL);
		}

		// TODO: something better here
		while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
		{
			LMS_LOG(PA, DEBUG) << "Still running...!";
			std::this_thread::yield();
		}
	}

	MainLoopLock lock {_mainLoop};

	if (!_stream)
		return {};

	if (pa_stream_get_state(_stream.get()) != PA_STREAM_READY)
	{
		LMS_LOG(PA, DEBUG) << "Stream not ready yet, skip get_time";
		return {};
	}

	pa_usec_t playTime {};
	int res {pa_stream_get_time(_stream.get(), &playTime)};
	if (res < 0)
		throw PulseAudioException {res, "pa_stream_get_time failed!"};

	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::microseconds {playTime});
}

std::chrono::milliseconds
PulseAudioOutput::getCurrentWriteTime()
{
	MainLoopLock lock {_mainLoop};

	if (!_stream)
		return {};

	if (pa_stream_get_state(_stream.get()) != PA_STREAM_READY)
	{
		LMS_LOG(PA, DEBUG) << "Stream not ready yet, skip get_time";
		return {};
	}

	const pa_timing_info* timingInfo {pa_stream_get_timing_info(_stream.get())};
	if (!timingInfo)
	{
		LMS_LOG(PA, DEBUG) << "pa_stream_get_timing_info: no data";
		return {};
	}

	return std::chrono::milliseconds {pa_bytes_to_usec(timingInfo->write_index, &_sampleSpec) / 1000};
}

