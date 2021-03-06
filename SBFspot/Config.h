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

#pragma once

#include "osselect.h"
#include "Types.h"

#include <boost/date_time/local_time/local_time.hpp>

struct StringConfig
{
    std::string name = "MPP";
    uint32_t inverterSerial = 0;
    float azimuth = 180.0f;
    float elevation = 30.0f;
    float powerPeak = 10000.0f;
};

enum class SqlType
{
    SqLite = 0,
    MySql = 1
};

struct SqlConfig
{
    SqlType     type = SqlType::SqLite;
    std::string databaseName = "~/smadata/SBFspot.db"; // Database name (MYSQL) or filename (SQLITE)
    std::string hostName;
    uint16_t    port = 3306;
    std::string userName;
    std::string password;
};

struct Config
{
    void parseAppPath(const char* appPath);
    int parseCmdline(int argc, char **argv);
    int readConfig();
    void showConfig();
    void sayHello(int ShowHelp);
    void invalidArg(char *arg);
    bool parseArrayProperty(const char *key, const char *value);

    std::string	ConfigFile;			//Fullpath to configuration file
    std::string	AppPath;
    char	BT_Address[18];			//Inverter bluetooth address 12:34:56:78:9A:BC
    std::vector<std::string> ip_addresslist; //List of Inverter IP addresses (for Speedwirecommunication )
    int		BT_Timeout = 5;
    int		BT_ConnectRetries = 10;
    short   IP_Port = 9522;
    CONNECTIONTYPE ConnectionType;     // CT_BLUETOOTH | CT_ETHERNET
    SmaUserGroup    smaUserGroup = UG_USER;    // USER|INSTALLER
    char	smaPassword[13];
    float	latitude = 0.0f;
    float	longitude = 0.0f;
    std::vector<StringConfig> pvArrays;    // Module array configurations
    uint16_t liveInterval = 60;
    uint16_t archiveInterval = 300;
    char	delimiter = ';';    // CSV field delimiter
    int		precision = 3;      // CSV value precision
    char	decimalpoint = ','; // CSV decimal point
    std::string outputPath;
    std::string outputPath_Events;
    std::string	plantname = "MyPlant";
    SqlConfig   sql;            // SQL specific config
    int		synchTime;				// 1=Synch inverter time with computer time (default=0)
    float	sunrise;
    float	sunset;
    int		calcMissingSpot = 0;    // 0-1
    char	DateTimeFormat[32];
    char	DateFormat[32];
    char	TimeFormat[32];
    int		CSV_Header;
    int		CSV_ExtendedHeader;
    int		CSV_SaveZeroPower;
    int		SunRSOffset;			// Offset to start before sunrise and end after sunset
    char	prgVersion[16];
    int		SpotTimeSource = 0;     // 0=Use inverter time; 1=Use PC time in Spot CSV
    int		SpotWebboxHeader = 0;   // 0=Use standard Spot CSV hdr; 1=Webbox style hdr
    char	locale[6];				// default en-US
    int		MIS_Enabled;			// Multi Inverter Support
    std::string	timezone;
    boost::local_time::time_zone_ptr tz;
    int		synchTimeLow;			// settime low limit
    int		synchTimeHigh;			// settime high limit

    // MQTT Stuff -- Using mosquitto (https://mosquitto.org/)
    std::string mqtt_publish_exe;	// default /usr/bin/mosquitto_pub ("%ProgramFiles%\mosquitto\mosquitto_pub.exe" on Windows)
    std::string mqtt_host = "localhost";
    uint16_t    mqtt_port = 1883;   // default 1883 (8883 for MQTT over TLS)
    std::string mqtt_topic = "sbfspot";
    std::string mqtt_publish_args = "-h {host} -t {topic} -m \"{{message}}\"";
    std::string mqtt_publish_data = "Timestamp,SunRise,SunSet,InvSerial,InvName,InvStatus,EToday,ETotal,PACTot,UDC1,UDC2,IDC1,IDC2,PDC1,PDC2";
    std::string mqtt_item_format = "\"{key}\": {value}";
    std::string mqtt_item_delimiter;// default comma

    //Commandline settings
    int		debug = 0;			// -d			Debug level (0-5)
    int		verbose = 0;        // -v			Verbose output level (0-5)
    int		archDays = 1;       // -ad			Number of days back to get Archived DayData (0=disabled, 1=today, ...)
    int		archMonths = 1;     // -am			Number of months back to get Archived MonthData (0=disabled, 1=this month, ...)
    int		archEventMonths = 1;    // -ae			Number of months back to get Archived Events (0=disabled, 1=this month, ...)
    bool    forceInq = false;   // -finq		Inquire inverter also during the night
    int		wsl = 0;            // -wsl			WebSolarLog support (http://www.websolarlog.com/index.php/tag/sma-spot/)
    int		quiet = 0;          // -q			Silent operation (No output except for -wsl)
    int		nospot = 0;         // -sp0			Disables Spot CSV export
    int		loadlive = 0;       // -loadlive	Force settings to prepare for live loading to http://pvoutput.org/loadlive.jsp
    time_t	startdate = 0;      // -startdate	Start reading of historic data at the given date (YYYYMMDD)
    S123_COMMAND    s123 = S123_NOP;    // -123s		123Solar Web Solar logger support(http://www.123solar.org/)

    enum class Command {
        Invalid,
        RunDaemon,
        SetTime,
        ImportHistoricalData,
        ImportDatabase
    };
    Command command = Command::Invalid;         // <command>    Command to execute

    std::set<ExporterType> exporters = { ExporterType::Csv, ExporterType::Sql };    // The exporters to use for publishing data.
};
