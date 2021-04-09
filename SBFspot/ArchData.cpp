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

#include "ArchData.h"

#include "Defines.h"
#include "Importer.h"
#include "SBFspot.h"
#include "misc.h"
#include "SBFNet.h"
#include "TagDefs.h"
#include "EventData.h"
#include "boost_ext.h"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/date_time/local_time/local_time.hpp"

using namespace std;
using namespace boost;
using namespace boost::date_time;
using namespace boost::posix_time;
using namespace boost::gregorian;

ArchData::ArchData(Importer& import) :
    m_import(import)
{
}

E_SBFSPOT ArchData::ArchiveDayData(std::vector<InverterData>& inverters, time_t startTime)
{
    if (VERBOSE_NORMAL)
    {
        puts("********************");
        puts("* ArchiveDayData() *");
        puts("********************");
    }

	bool hasMultigate = false;

	startTime -= 86400;		// fix Issue CP23: to overcome problem with DST transition - RB@20140330

    E_SBFSPOT rc = E_OK;
    struct tm start_tm;
    memcpy(&start_tm, localtime(&startTime), sizeof(start_tm));

    start_tm.tm_hour = 0;
    start_tm.tm_min = 0;
    start_tm.tm_sec = 0;
    start_tm.tm_mday++;		// fix Issue CP23: to overcome problem with DST transition - RB@20140330
	startTime = mktime(&start_tm);

    if (VERBOSE_NORMAL)
        printf("startTime = %08lX -> %s\n", startTime, strftime_t("%d/%m/%Y %H:%M:%S", startTime));

    for (auto& inverter : inverters)
	{
        if (inverter.SUSyID == SID_MULTIGATE)
            hasMultigate = true;

        inverter.hasDayData = false;
        for(unsigned int i=0; i<sizeof(inverter.dayData)/sizeof(DayData); i++)
		{
            DayData *pdayData = &inverter.dayData[i];
            pdayData->datetime = 0;
			pdayData->totalWh = 0;
			pdayData->watt = 0;
		}
	}

    int packetcount = 0;
    int validPcktID = 0;

    E_SBFSPOT hasData = E_ARCHNODATA;

    for (auto& inverter : inverters)
    {
        if ((inverter.DevClass != CommunicationProduct) && (inverter.SUSyID != SID_MULTIGATE))
		{
			do
			{
				pcktID++;
                writePacketHeader(pcktBuf, 0x01, inverter.BTAddress);
                writePacket(pcktBuf, 0x09, 0xE0, 0, inverter.SUSyID, inverter.Serial);
				writeLong(pcktBuf, 0x70000200);
				writeLong(pcktBuf, startTime - 300);
				writeLong(pcktBuf, startTime + 86100);
				writePacketTrailer(pcktBuf);
				writePacketLength(pcktBuf);
			}
			while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

            m_import.send(pcktBuf, inverter.IPAddress);

			do
			{
				unsigned long long totalWh = 0;
				unsigned long long totalWh_prev = 0;
				time_t datetime = 0;
				time_t datetime_prev = 0;
				time_t datetime_next = 0;
				bool dblrecord = false;		// Flag for double records (twins)

				const int recordsize = 12;

				do
				{
                    rc = m_import.getPacket(inverter.BTAddress, 1);
					if (rc != E_OK) return rc;

					packetcount = pcktBuf[25];

                    //TODO: Move checksum validation to bthGetPacket
					if ((ConnType == CT_BLUETOOTH) && (!validateChecksum()))
						return E_CHKSUM;
					else
					{
						unsigned short rcvpcktID = get_short(pcktBuf+27) & 0x7FFF;
						if ((validPcktID == 1) || (pcktID == rcvpcktID))
						{
							validPcktID = 1;
							for(int x = 41; x < (packetposition - 3); x += recordsize)
							{
								datetime_next = (time_t)get_long(pcktBuf + x);
								if (0 != (datetime_next - datetime)) // Fix Issue 108: sbfspot v307 crashes for daily export (-adnn)
								{
									totalWh_prev = totalWh;
									datetime_prev = datetime;
									datetime = datetime_next;
									dblrecord = false;
								}
								else
									dblrecord = true;

								totalWh = (unsigned long long)get_longlong(pcktBuf + x + 4);
								if (totalWh != NaN_U64) // Fix Issue 109: Bad request 400: Power value too high for system size
								{
									if (totalWh > 0) hasData = E_OK;
									if (totalWh_prev != 0)
									{
										struct tm timeinfo;
										memcpy(&timeinfo, localtime(&datetime), sizeof(timeinfo));
										if (start_tm.tm_mday == timeinfo.tm_mday)
										{
											unsigned int idx = (timeinfo.tm_hour * 12) + (timeinfo.tm_min / 5);
                                            if (idx < sizeof(inverter.dayData)/sizeof(DayData))
											{
												if (VERBOSE_HIGHEST && dblrecord)
												{
													std::cout << "Overwriting existing record: " << strftime_t("%d/%m/%Y %H:%M:%S", datetime);
                                                    std::cout << " - " << std::fixed << std::setprecision(3) << (double)inverter.dayData[idx].totalWh/1000 << "kWh";
                                                    std::cout << " - " << std::fixed << std::setprecision(0) << inverter.dayData[idx].watt << "W" << std::endl;
												}
												if (VERBOSE_HIGHEST && ((datetime - datetime_prev) > 300))
												{
													std::cout << "Missing records in datastream " << strftime_t("%d/%m/%Y %H:%M:%S", datetime_prev);
													std::cout << " -> " << strftime_t("%H:%M:%S", datetime) << std::endl;
												}
                                                inverter.dayData[idx].datetime = datetime;
                                                inverter.dayData[idx].totalWh = totalWh;
                                                //inverter.dayData[idx].watt = (totalWh - totalWh_prev) * 12;	// 60:5
												// Fix Issue 105 - Don't assume each interval is 5 mins
												// This is also a bug in SMA's Sunny Explorer V1.07.17 and before
                                                inverter.dayData[idx].watt = (totalWh - totalWh_prev) * 3600 / (datetime - datetime_prev);
                                                inverter.hasDayData = true;
											}
										}
									}
								}
							} //for
						}
						else
						{
							if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, rcvpcktID);
							validPcktID = 0;
							packetcount = 0;
						}
					}
				}
				while (packetcount > 0);
			}
			while (validPcktID == 0);
		}
    }

	if (hasMultigate)
	{
		/*
		 *	Consolidate micro-inverter daydata into multigate
		 *	For each multigate search its connected devices
		 *	Add totalWh and power of each device to multigate daydata
		 */
		
		if (VERBOSE_HIGHEST) std::cout << "Consolidating daydata of micro-inverters into multigate..." << std::endl;

        for (uint32_t mg=0; mg < inverters.size(); mg++)
		{
            InverterData& pmg = inverters[mg];
            if (pmg.SUSyID == SID_MULTIGATE)
			{
                pmg.hasDayData = true;
                for (uint32_t sb240=0; mg < inverters.size(); sb240++)
				{
                    const InverterData& psb = inverters[sb240];
                    if ((psb.SUSyID == SID_SB240) && (psb.multigateIndex == mg))
					{
                        for (unsigned int dd=0; dd < ARRAYSIZE(inverters[0].dayData); dd++)
						{
                            pmg.dayData[dd].datetime = psb.dayData[dd].datetime;
                            pmg.dayData[dd].totalWh += psb.dayData[dd].totalWh;
                            pmg.dayData[dd].watt += psb.dayData[dd].watt;
						}
					}
				}
			}
		}
	}

    return hasData;
}

E_SBFSPOT ArchData::ArchiveMonthData(std::vector<InverterData>& inverters, tm *start_tm)
{
    if (VERBOSE_NORMAL)
    {
        puts("**********************");
        puts("* ArchiveMonthData() *");
        puts("**********************");
    }

	bool hasMultigate = false;

    E_SBFSPOT rc = E_OK;

    // Set time to 1st of the month at 12:00:00
    start_tm->tm_hour = 12;
    start_tm->tm_min = 0;
    start_tm->tm_sec = 0;
    start_tm->tm_mday = 1;
    time_t startTime = mktime(start_tm);

    if (VERBOSE_NORMAL)
        printf("startTime = %08lX -> %s\n", startTime, strftime_t("%d/%m/%Y %H:%M:%S", startTime));

    for (auto& inverter : inverters)
	{
        if (inverter.SUSyID == SID_MULTIGATE) hasMultigate = true;
        inverter.hasMonthData = false;
        for(unsigned int i=0; i<sizeof(inverter.monthData)/sizeof(MonthData); i++)
		{
            inverter.monthData[i].datetime = 0;
            inverter.monthData[i].dayWh = 0;
            inverter.monthData[i].totalWh = 0;
		}
	}

    int packetcount = 0;
    int validPcktID = 0;

    for (auto& inverter : inverters)
    {
        if ((inverter.DevClass != CommunicationProduct) && (inverter.SUSyID != SID_MULTIGATE))
		{
			do
			{
				pcktID++;
                writePacketHeader(pcktBuf, 0x01, inverter.BTAddress);
                writePacket(pcktBuf, 0x09, 0xE0, 0, inverter.SUSyID, inverter.Serial);
				writeLong(pcktBuf, 0x70200200);
				writeLong(pcktBuf, startTime - 86400 - 86400);
                writeLong(pcktBuf, startTime + 86400 * (sizeof(inverter.monthData)/sizeof(MonthData) +1));
				writePacketTrailer(pcktBuf);
				writePacketLength(pcktBuf);
			}
			while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

            m_import.send(pcktBuf, inverter.IPAddress);

			do
			{
				unsigned long long totalWh = 0;
				unsigned long long totalWh_prev = 0;
				const int recordsize = 12;
				time_t datetime;

				unsigned int idx = 0;
				do
				{
                    rc = m_import.getPacket(inverter.BTAddress, 1);
					if (rc != E_OK) return rc;

                    //TODO: Move checksum validation to bthGetPacket
					if ((ConnType == CT_BLUETOOTH) && (!validateChecksum()))
						return E_CHKSUM;
					else
					{
						packetcount = pcktBuf[25];
						unsigned short rcvpcktID = get_short(pcktBuf+27) & 0x7FFF;
						if ((validPcktID == 1) || (pcktID == rcvpcktID))
						{
							validPcktID = 1;

							for(int x = 41; x < (packetposition - 3); x += recordsize)
							{
								datetime = (time_t)get_long(pcktBuf + x);
								//datetime -= (datetime % 86400) + 43200; // 3.0 - Round to UTC 12:00 - Removed 3.0.1 see issue C54
                                datetime += inverter.monthDataOffset; // Issues 115/130
								totalWh = get_longlong(pcktBuf + x + 4);
								if (totalWh != MAXULONGLONG)
								{
									if (totalWh_prev != 0)
									{
										struct tm utc_tm;
										memcpy(&utc_tm, gmtime(&datetime), sizeof(utc_tm));
										if (utc_tm.tm_mon == start_tm->tm_mon)
										{
                                            if (idx < sizeof(inverter.monthData)/sizeof(MonthData))
											{
                                                inverter.hasMonthData = true;
                                                inverter.monthData[idx].datetime = datetime;
                                                inverter.monthData[idx].totalWh = totalWh;
                                                inverter.monthData[idx].dayWh = totalWh - totalWh_prev;
												idx++;
											}
										}
									}
									totalWh_prev = totalWh;
								}
							} //for
						}
						else
						{
							if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, rcvpcktID);
							validPcktID = 0;
							packetcount = 0;
						}
					}
				}
				while (packetcount > 0);
			}
			while (validPcktID == 0);
		}
    }

	if (hasMultigate)
	{
		/*
		 *	Consolidate micro-inverter monthdata into multigate
		 *	For each multigate search its connected devices
		 *	Add totalWh and power of each device to multigate daydata
		 */

		if (VERBOSE_HIGHEST) std::cout << "Consolidating monthdata of micro-inverters into multigate..." << std::endl;

        for (uint32_t mg=0; mg < inverters.size(); mg++)
		{
            InverterData& pmg = inverters[mg];
            if (pmg.SUSyID == SID_MULTIGATE)
			{
                pmg.hasMonthData = true;
                for (uint32_t sb240=0; sb240 < inverters.size(); sb240++)
				{
                    const InverterData& psb = inverters[sb240];
                    if ((psb.SUSyID == SID_SB240) && (psb.multigateIndex == mg))
					{
                        for (unsigned int md=0; md < ARRAYSIZE(inverters[0].monthData); md++)
						{
                            pmg.monthData[md].datetime = psb.monthData[md].datetime;
                            pmg.monthData[md].totalWh += psb.monthData[md].totalWh;
                            pmg.monthData[md].dayWh += psb.monthData[md].dayWh;
						}
					}
				}
			}
		}
	}

    return E_OK;
}

E_SBFSPOT ArchData::ArchiveEventData(std::vector<InverterData>& inverters, boost::gregorian::date startDate, unsigned long UserGroup)
{
    E_SBFSPOT rc = E_OK;

    unsigned short pcktcount = 0;
    int validPcktID = 0;

	time_t startTime = to_time_t(startDate);
	time_t endTime = startTime + 86400 * startDate.end_of_month().day();

    for (auto& inverter : inverters)
    {
        do
        {
            pcktID++;
            writePacketHeader(pcktBuf, 0x01, inverter.BTAddress);
            writePacket(pcktBuf, 0x09, 0xE0, 0, inverter.SUSyID, inverter.Serial);
			writeLong(pcktBuf, UserGroup == UG_USER ? 0x70100200 : 0x70120200);
			writeLong(pcktBuf, startTime);
            writeLong(pcktBuf, endTime);
            writePacketTrailer(pcktBuf);
            writePacketLength(pcktBuf);
        }
        while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

        m_import.send(pcktBuf, inverter.IPAddress);

		bool FIRST_EVENT_FOUND = false;
        do
        {
            do
            {
                rc = m_import.getPacket(inverter.BTAddress, 1);
                if (rc != E_OK) return rc;

                //TODO: Move checksum validation to bthGetPacket
                if ((ConnType == CT_BLUETOOTH) && (!validateChecksum()))
                    return E_CHKSUM;
                else
                {
                    pcktcount = get_short(pcktBuf+25);
					unsigned short rcvpcktID = get_short(pcktBuf+27) & 0x7FFF;
                    if ((validPcktID == 1) || (pcktID == rcvpcktID))
                    {
                        validPcktID = 1;
                        for (int x = 41; x < (packetposition - 3); x += sizeof(SMA_EVENTDATA))
                        {
							SMA_EVENTDATA *pEventData = (SMA_EVENTDATA *)(pcktBuf + x);
							if (pEventData->DateTime > 0)	// Fix Issue 89
							{
                                inverter.eventData.push_back(EventData(UserGroup, pEventData));
								if (pEventData->EntryID == 1)
								{
									FIRST_EVENT_FOUND = true;
									rc = E_EOF;
								}
							}
						}

                    }
                    else
                    {
                        if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, rcvpcktID);
                        validPcktID = 0;
                        pcktcount = 0;
                    }
                }
            }
            while (pcktcount > 0);
        }
        while ((validPcktID == 0) && (!FIRST_EVENT_FOUND));
    }

    return rc;
}

//Issues 115/130
E_SBFSPOT ArchData::getMonthDataOffset(std::vector<InverterData>& inverters)
{
	E_SBFSPOT rc = E_OK;

	time_t now = time(NULL);
	struct tm now_tm;
	memcpy(&now_tm, gmtime(&now), sizeof(now_tm));

	// Temporarily disable verbose logging
	int currentVerboseLevel = verbose;
	verbose = 0;
	rc = ArchiveMonthData(inverters, &now_tm);
	verbose = currentVerboseLevel;

	if (rc == E_OK)
	{
        for (auto& inverter : inverters)
		{
            inverter.monthDataOffset = 0;
            if (inverter.hasMonthData)
			{
				// Get last record of monthdata
				for(int i = 30; i > 0; i--)
				{
                    if (inverter.monthData[i].datetime != 0)
					{
						now = time(NULL);
						memcpy(&now_tm, gmtime(&now), sizeof(now_tm));

						struct tm inv_tm;
                        memcpy(&inv_tm, gmtime(&inverter.monthData[i].datetime), sizeof(inv_tm));

						if (now_tm.tm_yday == inv_tm.tm_yday)
                            inverter.monthDataOffset = -86400;

						break;
					}
				}
			}
			if ((DEBUG_HIGHEST) && (!quiet))
                std::cout << inverter.SUSyID << ":" << inverter.Serial << " monthDataOffset=" << inverter.monthDataOffset << std::endl;
		}
	}

	return rc;
}
