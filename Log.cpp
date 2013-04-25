/*
	Copyright (c) 2013 Christopher A. Taylor.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of GCIF nor the names of its contributors may be used
	  to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include "Log.hpp"
#include "Clock.hpp"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <stdexcept>
using namespace std;
using namespace cat;

#if defined(CAT_OS_WINDOWS)
# include <process.h>
#endif

#if defined(CAT_ISA_X86)

#if defined(CAT_WORD_64) && defined(CAT_COMPILER_MSVC)
# define CAT_ARTIFICIAL_BREAKPOINT { ::DebugBreak(); }
#elif defined(CAT_ASM_INTEL)
# define CAT_ARTIFICIAL_BREAKPOINT { CAT_ASM_BEGIN int 3 CAT_ASM_END }
#elif defined(CAT_ASM_ATT)
# define CAT_ARTIFICIAL_BREAKPOINT { CAT_ASM_BEGIN "int $3" CAT_ASM_END }
#endif

#else
# define CAT_ARTIFICIAL_BREAKPOINT
#endif


static const char *const EVENT_NAME[5] = { "Inane", "Info", "Warn", "Oops", "Fatal" };
static const char *const SHORT_EVENT_NAME[5] = { ".", "I", "W", "!", "F" };


//// Free functions

std::string cat::HexDumpString(const void *vdata, u32 bytes)
{
	/* xxxx  xx xx xx xx xx xx xx xx  xx xx xx xx xx xx xx xx   aaaaaaaaaaaaaaaa*/

	const u8 *data = (const u8*)vdata;
	u32 ii, offset;

	char ascii[17];
	ascii[16] = 0;

	std::ostringstream oss;

	for (offset = 0; offset < bytes; offset += 16)
	{
		oss << endl << setfill('0') << hex << setw(4) << offset << "  ";

		for (ii = 0; ii < 16; ++ii)
		{
			if (ii == 8)
				oss << ' ';

			if (offset + ii < bytes)
			{
				u8 ch = data[offset + ii];

				oss << setw(2) << (u32)ch << ' ';
				ascii[ii] = (ch >= ' ' && ch <= '~') ? ch : '.';
			}
			else
			{
				oss << "   ";
				ascii[ii] = 0;
			}
		}

		oss << " " << ascii;
	}

	return oss.str();
}


//// Log

CAT_SINGLETON(Log);

bool Log::OnInitialize()
{
	_backend = Callback::FromMember<Log, &Log::DefaultLogCallback>(this);
#if defined(CAT_THREADED_LOGGER)
	_frontend = Callback::FromMember<Log, &Log::InvokeBackendAndSetupThreaded>(this);
#else
	_frontend = Callback::FromMember<Log, &Log::InvokeBackendAndUnlock>(this);
#endif
	_log_threshold = DEFAULT_LOG_LEVEL;

	return true;
}

#if defined(CAT_THREADED_LOGGER)

void Log::InvokeBackendAndSetupThreaded(EventSeverity severity, const char *source, const std::string &msg)
{
	// Bypass setup next time
	_frontend = Callback::FromMember<Log, &Log::InvokeBackendAndUnlock>(this);

	_backend(severity, source, msg);

	_lock.Leave();

	LogThread::ref();
}

#endif // CAT_THREADED_LOGGER

void Log::InvokeBackendAndUnlock(EventSeverity severity, const char *source, const std::string &msg)
{
	_backend(severity, source, msg);

	_lock.Leave();
}


void Log::FatalStop(const char *message)
{
	Log::ref()->InvokeBackend(LVL_FATAL, "FatalStop", message);

	CAT_ARTIFICIAL_BREAKPOINT;

	std::exit(EXIT_FAILURE);
}

void Log::InvokeFrontend(EventSeverity severity, const char *source, const std::string &msg)
{
	_lock.Enter();

	_frontend(severity, source, msg);
}

void Log::InvokeBackend(EventSeverity severity, const char *source, const std::string &msg)
{
	AutoMutex lock(_lock);

	_backend(severity, source, msg);
}

void Log::SetFrontend(const Callback &cb)
{
	AutoMutex lock(_lock);

	_frontend = cb;
}

void Log::ResetFrontend()
{
	AutoMutex lock(_lock);

	_frontend = Callback::FromMember<Log, &Log::InvokeBackendAndUnlock>(this);
}

void Log::SetBackend(const Callback &cb)
{
	AutoMutex lock(_lock);

	_backend = cb;
}

void Log::DefaultServiceCallback(EventSeverity severity, const char *source, const std::string &msg)
{
	std::ostringstream oss;
	oss << "<" << source << "> " << msg << endl;

	std::string result = oss.str();
	WriteServiceLog(severity, result.c_str());
}

void Log::DefaultLogCallback(EventSeverity severity, const char *source, const std::string &msg)
{
	std::ostringstream oss;
	oss << "[" << Clock::format("%b %d %H:%M") << "] <" << source << "> " << msg << endl;

	std::string result = oss.str();
	if (severity > LVL_WARN) cerr << result;
	else cout << result;

#if defined(CAT_OS_WINDOWS)
	OutputDebugStringA(result.c_str());
#endif
}

void Log::EnableServiceMode(const char *service_name)
{
	AutoMutex lock(_lock);

	_backend = Callback::FromMember<Log, &Log::DefaultServiceCallback>(this);

#if defined(CAT_OS_WINDOWS)
	_event_source = RegisterEventSourceA(0, service_name);
#endif
}

void Log::WriteServiceLog(EventSeverity severity, const char *line)
{
#if defined(CAT_OS_WINDOWS)
	if (_event_source)
	{
		WORD mode;

		switch (severity)
		{
		case LVL_INANE:	mode = EVENTLOG_SUCCESS; break;
		case LVL_INFO:	mode = EVENTLOG_INFORMATION_TYPE; break;
		case LVL_OOPS:
		case LVL_WARN:	mode = EVENTLOG_WARNING_TYPE; break;
		case LVL_FATAL:	mode = EVENTLOG_ERROR_TYPE; break;
		default: return;
		}

		ReportEventA(_event_source, mode, 0, mode, 0, 1, 0, &line, 0);
	}
#endif
}


//// Recorder

Recorder::Recorder(const char *source, EventSeverity severity)
{
	_source = source;
	_severity = severity;
}

Recorder::~Recorder()
{
	string msg = _msg.str();
	Log::ref()->InvokeFrontend(_severity, _source, msg);
}


//// Enforcer

Enforcer::Enforcer(const char *locus)
{
	oss << locus;
}

#if defined(CAT_COMPILER_MSVC)
#pragma warning(disable:4722) // Dtor never returns
#endif

Enforcer::~Enforcer()
{
	std::string result = oss.str();
	Log::FatalStop(result.c_str());
}
