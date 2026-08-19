/*
 * Copyright (C) 2026 Emeric Poupon
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

#include "AudioOutput.hpp"

#include <boost/asio/post.hpp>
#include <pulse/context.h>
#include <pulse/def.h>
#include <pulse/error.h>
#include <pulse/thread-mainloop.h>

#include "core/ILogger.hpp"
#include "core/LiteralString.hpp"

#include "audio/PcmTypes.hpp"

#include "AudioOutputStream.hpp"
#include "Exception.hpp"
#include "MainLoopScopedLock.hpp"

namespace lms::audio::pulseaudio
{
    namespace
    {
        core::LiteralString contextStateToString(pa_context_state_t state)
        {
            switch (state)
            {
            case PA_CONTEXT_UNCONNECTED: // The context hasn't been connected yet
                return "Unconnected";
            case PA_CONTEXT_CONNECTING: // A connection is being established
                return "Connecting";
            case PA_CONTEXT_AUTHORIZING: // The client is authorizing itself to the daemon
                return "Authorizing";
            case PA_CONTEXT_SETTING_NAME: // The client is passing its application name to the daemon
                return "Setting Name";
            case PA_CONTEXT_READY: // The connection is established, the context is ready to execute operations
                return "Ready";
            case PA_CONTEXT_FAILED: // The connection failed or was disconnected
                return "Failed";
            case PA_CONTEXT_TERMINATED: // The connection was terminated cleanly
                return "Terminated";
            }
            return "Unknown";
        }
    } // namespace

    void PaContextDeleter::operator()(pa_context* ctx) const noexcept
    {
        ::pa_context_unref(ctx);
    }

    void PaThreadedMainLoopDeleter::operator()(pa_threaded_mainloop* mainloop) const noexcept
    {
        ::pa_threaded_mainloop_free(mainloop);
    }

    AudioOutputContext::AudioOutputContext(boost::asio::io_context& ioContext, std::string_view name)
        : _ioContext{ ioContext }
    {
        _mainLoop = PaThreadedMainLoopPtr{ ::pa_threaded_mainloop_new() };
        if (!_mainLoop)
            throw Exception{ "pa_mainloop_new failed" };

        ::pa_mainloop_api* mainloop_api{ ::pa_threaded_mainloop_get_api(_mainLoop.get()) };

        _context = PaContextPtr{ ::pa_context_new(mainloop_api, std::string{ name }.c_str()) };
        if (!_context)
            throw Exception{ "pa_context_new failed" };

        ::pa_context_set_state_callback(_context.get(), [](pa_context*, void* userData) { static_cast<AudioOutputContext*>(userData)->onStateChanged(); }, this);

        {
            const int error{ ::pa_context_connect(_context.get(), nullptr, PA_CONTEXT_NOFLAGS, nullptr) };
            if (error < 0)
                throw PaException{ "pa_context_connect failed", error };
        }

        {
            const int error{ ::pa_threaded_mainloop_start(_mainLoop.get()) };
            if (error < 0)
                throw PaException{ "pa_threaded_mainloop_start failed", error };
        }
    }

    AudioOutputContext::~AudioOutputContext()
    {
        ::pa_threaded_mainloop_stop(_mainLoop.get());
    }

    void AudioOutputContext::asyncWaitReady(WaitReadyCallback cb)
    {
        MainLoopScopedLock lock{ _mainLoop.get() };

        if (pa_context_get_state(_context.get()) == PA_CONTEXT_READY)
        {
            boost::asio::post(_ioContext, std::move(cb));
        }
        else
        {
            _ioContext.get_executor().on_work_started();
            _waitReadyCallbacks.push_back(std::move(cb));
        }
    }

    std::unique_ptr<IAudioOutputStream> AudioOutputContext::createOutputStream(std::string_view name, const PcmParameters& outputParameters)
    {
        return std::make_unique<AudioOutputStream>(_ioContext, _context.get(), _mainLoop.get(), name, outputParameters);
    }

    void AudioOutputContext::onStateChanged()
    {
        const pa_context_state_t state{ pa_context_get_state(_context.get()) };
        LMS_LOG(AUDIO, DEBUG, "Context state changed to '" << contextStateToString(state) << "'");

        switch (state)
        {
        case PA_CONTEXT_READY:
            assert(pa_threaded_mainloop_in_thread(_mainLoop.get()));

            LMS_LOG(AUDIO, INFO, "Context connected to server '" << pa_context_get_server(_context.get()) << "'");

            for (auto& callback : _waitReadyCallbacks)
            {
                boost::asio::post(_ioContext, std::move(callback));
                // callback();
                _ioContext.get_executor().on_work_finished();
            }
            _waitReadyCallbacks.clear();

            break;

        default:
            break;
        }
    }
} // namespace lms::audio::pulseaudio