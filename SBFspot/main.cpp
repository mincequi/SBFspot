/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA solar inverters
    (c)2012-2021, SBF

    Latest version found at https://github.com/SBFspot/SBFspot

    License: Attribution-NonCommercial-ShareAlike 3.0 Unported (CC BY-NC-SA 3.0)
    http://creativecommons.org/licenses/by-nc-sa/3.0/

    You are free:
        to Share - to copy, distribute and transmit the work
        to Remix - to adapt the work
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

#include "osselect.h"

#include "Config.h"
#include "Ethernet.h"
#include "Importer.h"
#include "Inverter.h"
#include "SBFspot.h"
#include "TagDefs.h"
#include "Timer.h"
#include "misc.h"

#include <thread>

using namespace boost;

int main(int argc, char **argv)
{
#if defined(WIN32)
	// On Windows, switch console to UTF-8
	SetConsoleOutputCP(CP_UTF8);
#endif

    //Read config file and store settings in config struct
    Config config;
    config.parseAppPath(argv[0]);
    int rc = config.readConfig();	//Config struct contains fullpath to config file
    if (rc != 0) return rc;

    //Read the command line and store settings in config struct.
    //Note: this overrules settings that are provided by config file.
    rc = config.parseCmdline(argc, argv);
    if (rc == -1) return 1;	//Invalid commandline - Quit, error
    if (rc == 1) return 0;	//Nothing to do - Quit, no error

    //Copy some config settings to public variables
    debug = config.debug;
    verbose = config.verbose;
    quiet = config.quiet;
    ConnType = config.ConnectionType;

    if ((config.ConnectionType != CT_BLUETOOTH) && (config.settime == 1))
    {
        std::cout << "-settime is only supported for Bluetooth devices" << std::endl;
        return 0;
    }

    strncpy(DateTimeFormat, config.DateTimeFormat, sizeof(DateTimeFormat));
    strncpy(DateFormat, config.DateFormat, sizeof(DateFormat));

    if (VERBOSE_NORMAL) print_error(stdout, PROC_INFO, "Starting...\n");

    Timer timer(config);
    if (!timer.isBright() && (!config.forceInq) && (!config.daemon))
    {
        if (quiet == 0) puts("Nothing to do... it's dark. Use -finq to force inquiry.");
        return 0;
    }

    int status = tagdefs.readall(config.AppPath, config.locale);
    if (status != TagDefs::READ_OK)
    {
        printf("Error reading tags\n");
        return(2);
    }

    Ethernet ethernet;
    Importer import(config, ethernet);
    SbfSpot sbfSpot;
    Inverter inverter(config, ethernet, import, sbfSpot);
    do
    {
        bool isStartOfDay = false;
        auto timePoint = timer.nextTimePoint(&isStartOfDay);
        if (isStartOfDay)
        {
            inverter.reset();
        }
        std::this_thread::sleep_until(std::chrono::system_clock::from_time_t(timePoint));

        inverter.process(timePoint);
    }
    while(config.daemon);

    if (VERBOSE_NORMAL) print_error(stdout, PROC_INFO, "Done.\n");

    return 0;
}
