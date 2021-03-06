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

#include <array>
#include <ctime>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "EventData.h"

class Serial {
public:
    Serial(uint32_t serial);
    //Serial& operator=(uint32_t serial);

    operator uint32_t() const;

    uint32_t serial() const;

private:
    uint32_t m_serial;
};

enum class ExporterType : uint16_t {
    None = 0x0,
    Csv = 0x01,
    Sql = 0x02,
    Mqtt = 0x10,
    Ble = 0x20,
    LoRaWan = 0x40
};

enum CONNECTIONTYPE {
    CT_NONE = 0,
    CT_BLUETOOTH = 1,
    CT_ETHERNET  = 2
};

typedef enum
{
    S123_NOP = 0,	// Nop
    S123_DATA = 1,	// Send spot data frame
    S123_INFO = 2,	// Send program/inverter information
    S123_SYNC = 3,	// Synchronize inverter
    S123_STATE = 4	// Send inverter state data
} S123_COMMAND;

struct MonthData
{
    std::time_t datetime = 0;
    Serial serial = 0;
    long long totalWh = 0;  // changed to signed - issue 58
    long long dayWh = 0;    // changed to signed - issue 58
};

struct DayData
{
    std::time_t datetime = 0;
    Serial serial = 0;
    long long totalWh = 0;  // changed to signed - issue 58
    long long watt = 0;     // changed to signed - issue 58
};

// Holds statistics for a day
struct DayStats
{
    std::time_t timestamp = 0;  // [sec]
    Serial    serial = 0;
    float       powerMax = 0;
    std::vector<float> stringPowerMax;    // Maximum power per string
};

typedef struct
{
    unsigned short code;
    const char *meta;
    const char *fullText;
} CodeToMeta;

enum SmaInverterDataSet : uint32_t
{
    Invalid             = 0,
    EnergyProduction	= 1 << 0,
    SpotDCPower			= 1 << 1,
    SpotDCVoltage		= 1 << 2,
    SpotACPower			= 1 << 3,
    SpotACVoltage		= 1 << 4,
    SpotGridFrequency	= 1 << 5,
    MaxACPower			= 1 << 6,
    MaxACPower2			= 1 << 7,
    SpotACTotalPower	= 1 << 8,
    TypeLabel			= 1 << 9,
    OperationTime		= 1 << 10,
    SoftwareVersion		= 1 << 11,
    DeviceStatus		= 1 << 12,
    GridRelayStatus		= 1 << 13,
    BatteryChargeStatus = 1 << 14,
    BatteryInfo         = 1 << 15,
    InverterTemperature	= 1 << 16,
    MeteringGridMsTotW	= 1 << 17,
    HistoricDayDataRequest  = 0x70000200,
    HistoricDayDataResponse = 0x70000201,
    HistoricMonthDataRequest    = 0x70200200,
    HistoricMonthDataResponse   = 0x70200201,

    sbftest             = 1 << 30
};

enum SmaUserGroup : int32_t {
    UG_USER         = 0x07L,
    UG_INSTALLER    = 0x0AL
};

enum DEVICECLASS : uint16_t
{
    AllDevices = 8000,          // DevClss0
    SolarInverter = 8001,       // DevClss1
    WindTurbineInverter = 8002, // DevClss2
    BatteryInverter = 8007,     // DevClss7
    Consumer = 8033,            // DevClss33
    SensorSystem = 8064,        // DevClss64
    ElectricityMeter = 8065,    // DevClss65
    CommunicationProduct = 8128 // DevClss128
};

typedef enum
{
    DT_ULONG = 0,
    DT_STATUS = 8,
    DT_STRING = 16,
    DT_FLOAT = 32,
    DT_SLONG = 64
} SMA_DATATYPE;

class BluetoothAddress : public std::array<uint8_t, 6> {
    //using std::array<uint8_t, 6>::array;
};

struct InverterData
{
    std::string DeviceName;    //32 bytes + terminating zero
    //unsigned char BTAddress[6];
    BluetoothAddress BTAddress;
    std::string IPAddress;
    unsigned short SUSyID = 0;
    unsigned long serial = 0;
    unsigned char NetID = 0;
    float BT_Signal = 0.0f;
    time_t InverterDatetime = 0;
    time_t WakeupTime = 0;
    time_t SleepTime = 0;
    long Pdc1 = 0;
    long Pdc2 = 0;
    long Udc1 = 0;
    long Udc2 = 0;
    long Idc1 = 0;
    long Idc2 = 0;
    long Pmax1 = 0;
    long Pmax2 = 0;
    long Pmax3 = 0;
    long TotalPac = 0;
    long Pac1 = 0;
    long Pac2 = 0;
    long Pac3 = 0;
    long Uac1 = 0;
    long Uac2 = 0;
    long Uac3 = 0;
    long Iac1 = 0;
    long Iac2 = 0;
    long Iac3 = 0;
    long GridFreq = 0;
    long long OperationTime = 0;
    long long FeedInTime = 0;
    long long EToday = 0;
    long long ETotal = 0;
    unsigned short modelID = 0;
    std::string DeviceType;
    std::string DeviceClass;
    DEVICECLASS DevClass = AllDevices;
    std::string SWVersion;  //"03.01.05.R"
    int DeviceStatus = 0;
    int GridRelayStatus = 0;
    int flags = 0;
    std::array<DayData, 288> dayData;   // 24 * 60 / 5 (5 minute interval)
    MonthData monthData[31];
    bool hasMonthData = false;
    time_t monthDataOffset = 0;	// Issue 115
    std::vector<EventData> eventData;
    long calPdcTot = 0;
    long calPacTot = 0;
    float calEfficiency = 0;
    unsigned long BatChaStt = 0;        // Current battery charge status
    unsigned long BatDiagCapacThrpCnt = 0;  // Number of battery charge throughputs
    unsigned long BatDiagTotAhIn = 0;   // Amp hours counter for battery charge
    unsigned long BatDiagTotAhOut = 0;  // Amp hours counter for battery discharge
    unsigned long BatTmpVal = 0;        // Battery temperature
    unsigned long BatVol = 0;           // Battery voltage
    long BatAmp = 0;                    // Battery current
    long Temperature = 0;				// Inverter Temperature
    int32_t	MeteringGridMsTotWOut = 0;  // Power grid feed-in (Out)
    int32_t MeteringGridMsTotWIn = 0;   // Power grid reference (In)
    bool hasBattery = false;            // Smart Energy device
    uint32_t multigateIndex = std::numeric_limits<uint32_t>::max();
};

//SMA Structs must be aligned on byte boundaries
#pragma pack(push, 1)
typedef struct PacketHeader
{
    unsigned char  SOP;					// Start Of Packet (0x7E)
    unsigned short pkLength;
    unsigned char  pkChecksum;
    unsigned char  SourceAddr[6];		// SMA Inverter Address
    unsigned char  DestinationAddr[6];	// Local BT Address
    unsigned short command;
} pkHeader;

typedef struct
{
    uint32_t      MagicNumber;      // Packet signature 53 4d 41 00 (SMA\0)
    uint32_t      unknown1;         // 00 04 02 a0
    uint32_t      unknown2;         // 00 00 00 01
    unsigned char hiPacketLen;      // Packet length stored as big endian
    unsigned char loPacketLen ;     // Packet length Low Byte
} ethPacketHeaderL1;

typedef struct
{
    uint32_t      MagicNumber;      // Level 2 packet signature 00 10 60 65
    unsigned char longWords;        // int(PacketLen/4)
    unsigned char ctrl;
} ethPacketHeaderL2;

typedef struct
{
    ethPacketHeaderL1 pcktHdrL1;
    ethPacketHeaderL2 pcktHdrL2;
} ethPacketHeaderL1L2;

typedef struct
{
    uint16_t SUSyID;
    uint32_t serial;
    uint16_t Ctrl;  // unused
} ethEndpoint;

typedef struct
{
    unsigned char dummy0;
    ethPacketHeaderL2 pcktHdrL2;
    ethEndpoint Destination;
    ethEndpoint Source;
    unsigned short ErrorCode;
    unsigned short FragmentID;  //Count Down
    unsigned short PacketID;    //Count Up
} ethPacket;

typedef struct ArchiveDataRec
{
    time_t datetime;
    unsigned long long totalWh;
} ArchDataRec;
#pragma pack(pop)

enum LriDef : uint32_t
{
    OperationHealth                 = 0x00214800,   // *08* Condition (aka INV_STATUS)
    CoolsysTmpNom                   = 0x00237700,   // *40* Operating condition temperatures
    DcMsWatt                        = 0x00251E00,   // *40* DC power input (aka SPOT_PDC1 / SPOT_PDC2)
    MeteringTotWhOut                = 0x00260100,   // *00* Total yield (aka SPOT_ETOTAL)
    MeteringDyWhOut                 = 0x00262200,   // *00* Day yield (aka SPOT_ETODAY)
    GridMsTotW                      = 0x00263F00,   // *40* Power (aka SPOT_PACTOT)
    BatChaStt                       = 0x00295A00,   // *00* Current battery charge status
    OperationHealthSttOk            = 0x00411E00,   // *00* Nominal power in Ok Mode (aka INV_PACMAX1)
    OperationHealthSttWrn           = 0x00411F00,   // *00* Nominal power in Warning Mode (aka INV_PACMAX2)
    OperationHealthSttAlm           = 0x00412000,   // *00* Nominal power in Fault Mode (aka INV_PACMAX3)
    OperationGriSwStt               = 0x00416400,   // *08* Grid relay/contactor (aka INV_GRIDRELAY)
    OperationRmgTms                 = 0x00416600,   // *00* Waiting time until feed-in
    DcMsVol                         = 0x00451F00,   // *40* DC voltage input (aka SPOT_UDC1 / SPOT_UDC2)
    DcMsAmp                         = 0x00452100,   // *40* DC current input (aka SPOT_IDC1 / SPOT_IDC2)
    MeteringPvMsTotWhOut            = 0x00462300,   // *00* PV generation counter reading
    MeteringGridMsTotWhOut          = 0x00462400,   // *00* Grid feed-in counter reading
    MeteringGridMsTotWhIn           = 0x00462500,   // *00* Grid reference counter reading
    MeteringCsmpTotWhIn             = 0x00462600,   // *00* Meter reading consumption meter
    MeteringGridMsDyWhOut	        = 0x00462700,   // *00* ?
    MeteringGridMsDyWhIn            = 0x00462800,   // *00* ?
    MeteringTotOpTms                = 0x00462E00,   // *00* Operating time (aka SPOT_OPERTM)
    MeteringTotFeedTms              = 0x00462F00,   // *00* Feed-in time (aka SPOT_FEEDTM)
    MeteringGriFailTms              = 0x00463100,   // *00* Power outage
    MeteringWhIn                    = 0x00463A00,   // *00* Absorbed energy
    MeteringWhOut                   = 0x00463B00,   // *00* Released energy
    MeteringPvMsTotWOut             = 0x00463500,   // *40* PV power generated
    MeteringGridMsTotWOut           = 0x00463600,   // *40* Power grid feed-in
    MeteringGridMsTotWIn            = 0x00463700,   // *40* Power grid reference
    MeteringCsmpTotWIn              = 0x00463900,   // *40* Consumer power
    GridMsWphsA                     = 0x00464000,   // *40* Power L1 (aka SPOT_PAC1)
    GridMsWphsB                     = 0x00464100,   // *40* Power L2 (aka SPOT_PAC2)
    GridMsWphsC                     = 0x00464200,   // *40* Power L3 (aka SPOT_PAC3)
    GridMsPhVphsA                   = 0x00464800,   // *00* Grid voltage phase L1 (aka SPOT_UAC1)
    GridMsPhVphsB                   = 0x00464900,   // *00* Grid voltage phase L2 (aka SPOT_UAC2)
    GridMsPhVphsC                   = 0x00464A00,   // *00* Grid voltage phase L3 (aka SPOT_UAC3)
    GridMsAphsA_1                   = 0x00465000,   // *00* Grid current phase L1 (aka SPOT_IAC1)
    GridMsAphsB_1                   = 0x00465100,   // *00* Grid current phase L2 (aka SPOT_IAC2)
    GridMsAphsC_1                   = 0x00465200,   // *00* Grid current phase L3 (aka SPOT_IAC3)
    GridMsAphsA                     = 0x00465300,   // *00* Grid current phase L1 (aka SPOT_IAC1_2)
    GridMsAphsB                     = 0x00465400,   // *00* Grid current phase L2 (aka SPOT_IAC2_2)
    GridMsAphsC                     = 0x00465500,   // *00* Grid current phase L3 (aka SPOT_IAC3_2)
    GridMsHz                        = 0x00465700,   // *00* Grid frequency (aka SPOT_FREQ)
    MeteringSelfCsmpSelfCsmpWh      = 0x0046AA00,   // *00* Energy consumed internally
    MeteringSelfCsmpActlSelfCsmp    = 0x0046AB00,   // *00* Current self-consumption
    MeteringSelfCsmpSelfCsmpInc     = 0x0046AC00,   // *00* Current rise in self-consumption
    MeteringSelfCsmpAbsSelfCsmpInc  = 0x0046AD00,   // *00* Rise in self-consumption
    MeteringSelfCsmpDySelfCsmpInc   = 0x0046AE00,   // *00* Rise in self-consumption today
    BatDiagCapacThrpCnt             = 0x00491E00,   // *40* Number of battery charge throughputs
    BatDiagTotAhIn                  = 0x00492600,   // *00* Amp hours counter for battery charge
    BatDiagTotAhOut                 = 0x00492700,   // *00* Amp hours counter for battery discharge
    BatTmpVal                       = 0x00495B00,   // *40* Battery temperature
    BatVol                          = 0x00495C00,   // *40* Battery voltage
    BatAmp                          = 0x00495D00,   // *40* Battery current
    NameplateLocation               = 0x00821E00,   // *10* Device name (aka INV_NAME)
    NameplateMainModel              = 0x00821F00,   // *08* Device class (aka INV_CLASS)
    NameplateModel                  = 0x00822000,   // *08* Device type (aka INV_TYPE)
    NameplateAvalGrpUsr             = 0x00822100,   // *  * Unknown
    NameplatePkgRev                 = 0x00823400,   // *08* Software package (aka INV_SWVER)
    InverterWLim                    = 0x00832A00,   // *00* Maximum active power device (aka INV_PACMAX1_2) (Some inverters like SB3300/SB1200)
    GridMsPhVphsA2B6100             = 0x00464B00,
    GridMsPhVphsB2C6100             = 0x00464C00,
    GridMsPhVphsC2A6100             = 0x00464D00
};

typedef enum
{
    E_OK			=  0,
    E_NODATA		= -1,	// Bluetooth buffer empty
    E_BADARG		= -2,	// Unknown command line argument
    E_CHKSUM		= -3,	// Invalid Checksum
    E_BUFOVRFLW		= -4,	// Buffer overflow
    E_ARCHNODATA	= -5,	// No archived data found for given timespan
    E_INIT			= -6,	// Unable to initialize
    E_INVPASSW		= -7,	// Invalid password
    E_RETRY			= -8,	// Retry the last action
    E_EOF			= -9,	// End of data
    E_PRIVILEGE		= -10,	// Privilege not held (need installer login)
    E_LOGONFAILED	= -11,	// Logon failed, other than Invalid Password (E_INVPASSW)
    E_COMM			= -12	// General communication error
} E_SBFSPOT;

using DataPerInverter = std::map<uint32_t, std::list<InverterData>>;
using InverterDataMap = std::map<LriDef, float>;

class ByteBuffer : public std::vector<uint8_t> {
public:
    using std::vector<uint8_t>::vector;
    using std::vector<uint8_t>::operator=;

    // These functions are stateful reader functions. Always call reset() when reading from the beginning.
    void reset();
    uint8_t readUint8();
    std::vector<uint8_t> readByteArray(uint16_t size);
    uint16_t readUint16(bool doByteSwap = true);
    uint32_t readUint32(bool doByteSwap = true);

private:
    size_t m_currentPosition = 0;
};
