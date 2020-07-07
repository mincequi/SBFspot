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
#include "filetools.h"
#define BOOST_FILESYSTEM_NO_DEPRECATED

#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"
#include <boost/algorithm/string.hpp>
#include <string>
#include <vector>
#include <stdio.h> //fopen/fclose

namespace fs = boost::filesystem;

namespace sbf
{
void filetools::terminatepath(std::string &dir)
{
	if (!dir.empty())
	{
		char lastchar = *dir.rbegin();

		if ((lastchar != '/') && (lastchar != '\\'))
			dir += FOLDERSEPARATOR;
	}
}

bool filetools::create_directories(const std::string path)
{
	return boost::filesystem::create_directories(path);
}

int filetools::expandpath(std::string& path)
{
	char buf[256];
	time_t now = time(NULL);
	int rc = strftime(buf, sizeof(buf)-1, path.c_str(), localtime(&now));
	path = buf;
	return rc;
}

bool filetools::iswritable(const char *fullpath)
{
	FILE *f;
	if ((f = fopen(fullpath, "a+")) == NULL)
		return false;
	else
	{
		fclose(f);
		return true;
	}
}

bool filetools::pathexists(const std::string dir)
{
	fs::path p(dir);
	return fs::exists(p);
}

int filetools::getfiles(FILELIST& filelist, const std::string path, const std::string filter, const bool recursive)
{
	fs::path full_path(fs::initial_path<fs::path>());
	full_path = fs::system_complete(fs::path(path));

	if (fs::is_directory(full_path))
	{
		fs::directory_iterator end_iter;
		for (fs::directory_iterator dir_itr(full_path); dir_itr != end_iter; ++dir_itr)
		{
			try
			{
				if (fs::is_directory(dir_itr->status()))
				{
					if (recursive)
						getfiles(filelist, dir_itr->path().string(), filter, recursive);
				}
				else if (fs::is_regular_file(dir_itr->status()))
				{
					if (boost::iends_with(dir_itr->path().string(), filter))
						filelist.push_back(dir_itr->path().string());
				}
			}
			catch (...)
			{
                return -1;
			}
		}
	}
	return 0;
}

} //namespace sbf
