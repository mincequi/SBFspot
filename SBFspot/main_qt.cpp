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

#include "Cache.h"
#include "Config.h"
#include "ExporterManager.h"
#include "Logging.h"
#include "sma/SmaManager.h"

int main(int argc, char *argv[])
{
    Logging::init(argc, argv);

    // Read the command line and store settings in config struct.
    Config config;
    int rc = config.parseCmdline(argc, argv);
    if (rc == -1) return 1;	// Invalid commandline - Quit, error
    if (rc == 1) return 0;  // Nothing to do - Quit, no error

    // Read config file and store settings in config struct.
    rc = config.readConfig();   // Config struct contains fullpath to config file
    if (rc != 0) return rc;

    // We are always a daemon.
    config.daemon = true;

    // We default to Ethernet for now.
    ConnType = CT_ETHERNET;

    // Create application instance
    QCoreApplication app(argc, argv);

    // The cache is the central object to hold all the data that is coming in
    // and going out. It is be fed by importers (SmaManager/SmaInverter) and
    // feeds exporters (like ExporterManager or MqttExport).
    Cache cache;
    // The exporterManager is the central object to hold all the exporters like
    // MqttExporter, CsvExporter, SqlExporter.
    ExporterManager exporterManager(config, cache);
    // The deviceManager serves as an abstraction for all interactions with the
    // actual inverters, energy meters and other devices. For our case all SMA
    // specific stuff is abstracted with it.
    sma::SmaManager deviceManager(config, exporterManager);
    // This call will discover all inverters within the network and starts
    // polling them. User configuration (like intervals, password) is respected
    // for this. No further interaction is needed.
    deviceManager.discoverDevices();

    // Start main loop.
    return app.exec();
}
