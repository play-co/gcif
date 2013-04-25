/*
	Copyright (c) 2013 Game Closure.  All rights reserved.

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

#ifndef CAT_LOG_HPP
#define CAT_LOG_HPP

#include "Delegates.hpp"
#include "Singleton.hpp"
#include "Mutex.hpp"
#include <string>
#include <sstream>

namespace cat {


class Log;
class LogThread;
class Recorder;
class Enforcer;


//// Enumerations

enum EventSeverity
{
	LVL_INANE,
	LVL_INFO,
	LVL_WARN,
	LVL_OOPS,
	LVL_FATAL,

	LVL_SILENT, // invalid for an actual event's level, valid value for a threshold
};

#if defined(CAT_DEBUG)
	static const int DEFAULT_LOG_LEVEL = LVL_INANE;
#else
	static const int DEFAULT_LOG_LEVEL = LVL_INFO;
#endif



//// Utility

std::string CAT_EXPORT HexDumpString(const void *vdata, u32 bytes);


//// Log

class CAT_EXPORT Log : public Singleton<Log>
{
	friend class Recorder;
	friend class LogThread;

	bool OnInitialize();

public:
	typedef Delegate3<void, EventSeverity, const char *, const std::string &> Callback;

private:
	Mutex _lock;
	Callback _backend;	// Callback that actually does the output
	Callback _frontend;	// Hook that might be set to the backend, but could also be used to redirect output
	int _log_threshold;
#if defined(CAT_OS_WINDOWS)
	HANDLE _event_source;
#endif

	void InvokeFrontend(EventSeverity severity, const char *source, const std::string &msg);
	void InvokeBackend(EventSeverity severity, const char *source, const std::string &msg);

	void SetFrontend(const Callback &cb);
	void ResetFrontend();

#if defined(CAT_THREADED_LOGGER)
	void InvokeBackendAndSetupThreaded(EventSeverity severity, const char *source, const std::string &msg);
#endif
	void InvokeBackendAndUnlock(EventSeverity severity, const char *source, const std::string &msg);

public:
	CAT_INLINE void SetThreshold(EventSeverity min_severity) { _log_threshold = min_severity; }
	CAT_INLINE int GetThreshold() { return _log_threshold; }

	// Write to console (and debug log in windows) then trigger a breakpoint and exit
	static void FatalStop(const char *message);

	// Service mode
	void EnableServiceMode(const char *service_name);
	void WriteServiceLog(EventSeverity severity, const char *line);

	void SetBackend(const Callback &cb);
	void DefaultServiceCallback(EventSeverity severity, const char *source, const std::string &msg);
	void DefaultLogCallback(EventSeverity severity, const char *source, const std::string &msg);
};


//// Recorder

class CAT_EXPORT Recorder
{
	CAT_NO_COPY(Recorder);

	friend class Log;

	EventSeverity _severity;
	const char *_source;
	std::ostringstream _msg;

public:
	Recorder(const char *source, EventSeverity severity);
	~Recorder();

public:
	template<class T> inline Recorder &operator<<(const T &t)
	{
		_msg << t;
		return *this;
	}
};

// Because there is an IF statement in the macro, you cannot use the
// braceless if-else construction:
//  if (XYZ) WARN("SS") << "ERROR!"; else INFO("SS") << "OK!";	   <-- bad
// Instead use:
//  if (XYZ) { WARN("SS") << "ERROR!"; } else INFO("SS") << "OK!";   <-- good
#define CAT_RECORD(subsystem, severity) \
	if (severity >= Log::ref()->GetThreshold()) Recorder(subsystem, severity)

#define CAT_INANE(subsystem)	CAT_RECORD(subsystem, cat::LVL_INANE)
#define CAT_INFO(subsystem)		CAT_RECORD(subsystem, cat::LVL_INFO)
#define CAT_WARN(subsystem)		CAT_RECORD(subsystem, cat::LVL_WARN)
#define CAT_OOPS(subsystem)		CAT_RECORD(subsystem, cat::LVL_OOPS)
#define CAT_FATAL(subsystem)	CAT_RECORD(subsystem, cat::LVL_FATAL)

// If not in debug mode, conditionally compile-out logging modes
#if !defined(CAT_DEBUG)

# if defined(CAT_RELEASE_DISABLE_INANE)
#undef CAT_INANE
#define CAT_INANE(subsystem)	while (false) Recorder(subsystem, cat::LVL_INANE)
# endif // CAT_RELEASE_DISABLE_INANE

# if defined(CAT_RELEASE_DISABLE_INFO)
#undef CAT_INFO
#define CAT_INFO(subsystem)		while (false) Recorder(subsystem, cat::LVL_INFO)
# endif // CAT_RELEASE_DISABLE_INFO

# if defined(CAT_RELEASE_DISABLE_WARN)
#undef CAT_WARN
#define CAT_WARN(subsystem)		while (false) Recorder(subsystem, cat::LVL_WARN)
# endif // CAT_RELEASE_DISABLE_WARN

# if defined(CAT_RELEASE_DISABLE_OOPS)
#undef CAT_OOPS
#define CAT_OOPS(subsystem)		while (false) Recorder(subsystem, cat::LVL_OOPS)
# endif // CAT_RELEASE_DISABLE_OOPS

# if defined(CAT_RELEASE_DISABLE_FATAL)
#undef CAT_FATAL
#define CAT_FATAL(subsystem)	while (false) Recorder(subsystem, cat::LVL_FATAL)
# endif // CAT_RELEASE_DISABLE_FATAL

#endif // !CAT_DEBUG


//// Enforcer

class CAT_EXPORT Enforcer
{
	CAT_NO_COPY(Enforcer);

protected:
	std::ostringstream oss;

public:
	Enforcer(const char *locus);
	~Enforcer();

public:
	template<class T> inline Enforcer &operator<<(const T &t)
	{
		oss << t;
		return *this;
	}
};


#define CAT_USE_ENFORCE_EXPRESSION_STRING
#define CAT_USE_ENFORCE_FILE_LINE_STRING


#if defined(CAT_USE_ENFORCE_EXPRESSION_STRING)
# define CAT_ENFORCE_EXPRESSION_STRING(exp) "Failed assertion (" #exp ")"
#else
# define CAT_ENFORCE_EXPRESSION_STRING(exp) "Failed assertion"
#endif

#if defined(CAT_USE_ENFORCE_FILE_LINE_STRING)
# define CAT_ENFORCE_FILE_LINE_STRING " at " CAT_FILE_LINE_STRING
#else
# define CAT_ENFORCE_FILE_LINE_STRING ""
#endif

// Because there is an IF statement in the macro, you cannot use the
// braceless if-else construction:
//  if (XYZ) ENFORCE(A == B) << "ERROR"; else INFO("SS") << "OK";	   <-- bad!
// Instead use:
//  if (XYZ) { ENFORCE(A == B) << "ERROR"; } else INFO("SS") << "OK";   <-- good!

#define CAT_ENFORCE(exp) if ( (exp) == 0 ) Enforcer(CAT_ENFORCE_EXPRESSION_STRING(exp) CAT_ENFORCE_FILE_LINE_STRING "\n")
#define CAT_EXCEPTION() Enforcer("Exception" CAT_ENFORCE_FILE_LINE_STRING "\n")

#if defined(CAT_DEBUG)
# define CAT_DEBUG_ENFORCE(exp) CAT_ENFORCE(exp)
# define CAT_DEBUG_EXCEPTION() CAT_EXCEPTION()
#else
# define CAT_DEBUG_ENFORCE(exp) while (false) CAT_ENFORCE(1) /* hopefully will be optimized out of existence */
# define CAT_DEBUG_EXCEPTION()
#endif


} // namespace cat

#endif // CAT_LOG_HPP
