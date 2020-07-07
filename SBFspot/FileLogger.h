/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA® solar inverters
	(c)2012-2018, SBF

	Latest version found at https://github.com/SBFspot/SBFspot

	License: Attribution-NonCommercial-ShareAlike 3.0 Unported (CC BY-NC-SA 3.0)
	http://creativecommons.org/licenses/by-nc-sa/3.0/

	You are free:
		to Share — to copy, distribute and transmit the work
		to Remix — to adapt the work
	Under the following conditions:
	Attribution:
		You must attribute the work in the manner specified by the author or licensor
		(but not in any way that suggests that they endorse you or your use of the work).
	Noncommercial:
		You may not use this work for commercial purposes.
	Share Alike:
		If you alter, transform, or build upon this work, you may distribute the resulting work
		only under the same or similar license to this one.

DISCLAIMER:
	A user of SBFspot software acknowledges that he or she is receiving this
	software on an "as is" basis and the user is not relying on the accuracy
	or functionality of the software for any purpose. The user further
	acknowledges that any use of this software will be at his own risk
	and the copyright owner accepts no responsibility whatsoever arising from
	the use or application of the software.

	SMA is a registered trademark of SMA Solar Technology AG

************************************************************************************************/
#pragma once

#include "osselect.h"
#include "filetools.h"
#include <string>
#include <fstream>

#if defined(_WIN32)
	#if !defined(AF_IPX)
		#include <winsock2.h>
	#endif
	#include <windows.h>
#endif

#if defined(__linux__)
#include <sys/time.h>
#endif

namespace sbf
{

#define TIMESTAMP_FORMAT_WITHDATE 1

    namespace flog
    {
        typedef enum
        {
            FLOG_DEBUG = 0,
            FLOG_INFO,
            FLOG_WARNING,
            FLOG_ERROR,
            FLOG_CRITICAL,
            FLOG_MSGONLY
        } E_FLOGTYPE;
    }

	static const char *logTypes[] = {"DBG", "INF", "WRN", "ERR", "CRITICAL", ""};

#if defined(TIMESTAMP_FORMAT_WITHDATE)
	static const char *timestampformat = "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ";
#else
	static const char *timestampformat = "[%02d:%02d:%02d.%03d] ";
#endif

	class FileLogger
	{
	private:
		std::string m_logpath;
		std::string m_logname;
		std::string m_fullpath;
		std::ofstream m_ofstream;
		flog::E_FLOGTYPE m_minloglevel;
		flog::E_FLOGTYPE m_curloglevel;

		unsigned int m_wrn_count;
		unsigned int m_err_count;

	public:
		FileLogger(const char *logpath, const char *logname, const flog::E_FLOGTYPE loglevel)
		{
			m_logpath = logpath;
			m_logname = logname;
			m_minloglevel = m_curloglevel = loglevel;
			resetcounters();
			check_open();
		}

		FileLogger(const std::string logpath, const std::string logname, const flog::E_FLOGTYPE loglevel)
		{
			m_logpath = logpath;
			m_logname = logname;
			m_minloglevel = m_curloglevel = loglevel;
			resetcounters();
			check_open();
		}

        ~FileLogger()
		{
			if (m_ofstream.is_open())
				m_ofstream.close();
        }

		std::string fullpath() const { return m_fullpath; }

		bool is_open() { return m_ofstream.is_open(); }

		void resetcounters() { m_wrn_count = m_err_count = 0; }
		unsigned int errorcount() const { return m_err_count; }
		unsigned int warningcount() const { return m_wrn_count; }

		bool check_open()
		{
			if (!m_ofstream.is_open())
			{
				if (m_fullpath.empty())
				{
					std::string logpath = m_logpath;
					if (filetools::expandpath(logpath) != 0)
					{
						filetools::create_directories(logpath);
						// m_logname can be expandable (%Y) too
						m_fullpath = logpath + m_logname;
						filetools::expandpath(m_fullpath);
					}
					else
						return false;
				}
				m_ofstream.open(m_fullpath.c_str(), std::ios::app | std::ios::out);
			}

			return m_ofstream.is_open();
		}

		std::string timestamp()
		{
			char buffer[64];
#if defined(__linux__)
			// Get a timestamp
			struct timeval tv;
			gettimeofday(&tv, NULL);
			struct tm *tm;
			tm = localtime(&tv.tv_sec);

			// create a time stamp string for the log message
			snprintf(buffer, sizeof(buffer), timestampformat,
#if defined(TIMESTAMP_FORMAT_WITHDATE)
				tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
#endif
				tm->tm_hour, tm->tm_min, tm->tm_sec, (int)tv.tv_usec / 1000);
#elif defined(_WIN32)
			// Get a timestamp
			SYSTEMTIME time;
			::GetLocalTime(&time);

			// create a time stamp string for the log message
			sprintf_s(buffer, sizeof(buffer), timestampformat,
#if defined(TIMESTAMP_FORMAT_WITHDATE)
				time.wYear, time.wMonth, time.wDay,
#endif
				time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
#endif

			std::string sTimestamp(buffer);

			return sTimestamp;
		}

        friend FileLogger& operator << (FileLogger& logger, const flog::E_FLOGTYPE logtype)
		{
			if (logger.check_open())
			{
				if (logtype == flog::FLOG_WARNING) logger.m_wrn_count++;
				if (logtype == flog::FLOG_ERROR) logger.m_err_count++;

				logger.m_curloglevel = logtype;
				if ((logger.m_curloglevel >= logger.m_minloglevel) && (logger.m_curloglevel != flog::FLOG_MSGONLY))
					logger.m_ofstream << logger.timestamp() << logTypes[logtype] << ": ";
			}

			return logger;
        }


		// std::endl
		friend FileLogger& operator << (FileLogger& logger, std::ostream& (*fn)(std::ostream&))
		{
			if (logger.check_open())
			{
				if (logger.m_curloglevel >= logger.m_minloglevel)
					logger.m_ofstream << fn;
			}

			logger.m_ofstream.close();

			return logger;
		}

		template<class T>
		friend FileLogger& operator << (FileLogger& logger, const T& t)
		{
			if (logger.check_open())
			{
				if (logger.m_curloglevel >= logger.m_minloglevel)
					logger.m_ofstream << t;
			}

			return logger;
		}

		// non construction-copyable
		FileLogger(const FileLogger& other);
		// non copyable
		FileLogger& operator=(const FileLogger&);

    };
}
