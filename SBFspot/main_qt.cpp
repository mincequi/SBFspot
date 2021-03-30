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

#include <QCoreApplication>

#include "Config.h"
#include "Ethernet_qt.h"
#include "Processor.h"

int main(int argc, char *argv[])
{
    // Read the command line and store settings in config struct
    Config config;
    int rc = config.parseCmdline(argc, argv);
    if (rc == -1) return 1;	// Invalid commandline - Quit, error
    if (rc == 1) return 0;  // Nothing to do - Quit, no error

    // Read config file and store settings in config struct
    rc = config.readConfig();   // Config struct contains fullpath to config file
    if (rc != 0) return rc;

    QCoreApplication app(argc, argv);

    Processor processor(config);
    Ethernet_qt ethernet(processor);

    return app.exec();
}
