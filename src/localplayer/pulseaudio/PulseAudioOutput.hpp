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

#pragma once

#include "PulseAudioOutput.hpp"

#include <memory>
#include <thread>

#include <pulse/context.h>
#include <pulse/mainloop.h>
#include <pulse/stream.h>

#include "av/AvTranscoder.hpp"
#include "database/Session.hpp"
#include "localplayer/AudioOutput.hpp"

class PulseAudioOutput final : public AudioOutput
{
	public:
		PulseAudioOutput(Format format, SampleRate sampleRate, std::size_t nbChannels);
		~PulseAudioOutput() = default;

		PulseAudioOutput (const PulseAudioOutput &) = delete;
		PulseAudioOutput (PulseAudioOutput &&) = delete;
		PulseAudioOutput & operator=(const PulseAudioOutput &) = delete;
		PulseAudioOutput & operator=(PulseAudioOutput &&) = delete;

	private:

		// AudioOutput
		Format		getFormat() const override		{ return _format; }
		SampleRate	getSampleRate() const override		{ return _sampleRate; }
		std::size_t	nbChannels() const override		{ return _nbChannels; }

		void		start() override;
		void		stop() override;
		void		resume() override {}
		void		pause() override {}
		void		setVolume(Volume volume) override {}
		void		flush() override {}
		void		setOnCanWriteCallback(OnCanWriteCallback cb) override;
		std::size_t	getCanWriteBytes() override;
		std::size_t	write(const unsigned char* data, std::size_t size) override;
	

		void threadEntry();

		void connect();
		void disconnect();
		void onContextStateChanged();

		void createStream();
		void connectStream();
		void onStreamStateChanged();
		void onStreamCanWrite(std::size_t nbMaxBytes);

		const Format		_format {Format::S16LE}; // TODO: make it configurable
		const SampleRate	_sampleRate {44100}; // TODO: make it configurable
		const std::size_t	_nbChannels {2};
		const pa_sample_spec	_sampleSpec;

		std::unique_ptr<std::thread> _thread;

		OnCanWriteCallback 	_onCanWriteCallback;

		struct MainLoopDeleter
		{
			void operator()(pa_mainloop* obj)
			{
				pa_mainloop_free(obj);
			}
		};
		using MainLoopPtr = std::unique_ptr<::pa_mainloop, MainLoopDeleter>;
		MainLoopPtr _mainLoop;

		struct ContextDeleter
		{
			void operator()(pa_context* obj)
			{
				pa_context_unref(obj);
			}
		};
		using ContextPtr = std::unique_ptr<::pa_context, ContextDeleter>;
		ContextPtr _context;

		struct StreamDeleter
		{
			void operator()(pa_stream* obj )
			{
				pa_stream_unref(obj);
			}
		};
		using StreamPtr = std::unique_ptr<::pa_stream, StreamDeleter>;
		StreamPtr _stream;

};


