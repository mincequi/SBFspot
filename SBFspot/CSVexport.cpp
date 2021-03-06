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

#include "CSVexport.h"

#include "Config.h"
#include "Defines.h"
#include "Logger.h"
#include "TagDefs.h"
#include "misc.h"

using namespace std;

CsvExporter::CsvExporter(const Config& config) :
    m_config(config)
{
}

//Linebreak To Text
const char *CsvExporter::linebreak2txt(void)
{
#if defined (WIN32)
	return "CR/LF";
#endif

#if defined (linux) || defined (__APPLE__)
	return "LF";
#endif
}

// Convert format string like %d/%m/%Y %H:%M:%S to dd/mm/yyyy HH:mm:ss
// Returns a pointer to DMY string
// Caller is responsible to free the memory
char *CsvExporter::DateTimeFormatToDMY(const char *dtf)
{
	char DMY[80] = {0};
	char ch[2];
	for (int i = 0; (dtf[i] != 0) && (strlen(DMY) < sizeof(DMY)-4); i++)
	{
		if (dtf[i] == '%')
		{
			i++;
			switch (dtf[i])
			{
			case 0:	// end-of-string
				i--; break;
			case 'y':
				strcat(DMY, "yy"); break;
			case 'Y':
				strcat(DMY, "yyyy"); break;
			case 'm':
				strcat(DMY, "MM"); break;
			case 'd':
				strcat(DMY, "dd"); break;
			case 'H':
				strcat(DMY, "HH"); break;
			case 'M':
				strcat(DMY, "mm"); break;
			case 'S':
				strcat(DMY, "ss"); break; //Fix Issue 84
			default:
				ch[0] = dtf[i]; ch[1] = 0;
				strcat(DMY, ch);
			}
		}
		else
		{
				ch[0] = dtf[i]; ch[1] = 0;
				strcat(DMY, ch);
		}
	}

	return strdup(DMY);
}

void CsvExporter::exportMonthData(const std::vector<InverterData>& inverters)
{
	char msg[80 + MAX_PATH];
    if (m_config.exporters.count(ExporterType::Csv) == 1)
	{
		if (VERBOSE_NORMAL) puts("ExportMonthDataToCSV()");

        if (inverters[0].monthData[0].datetime <= 0)   //invalid date?
        {
            if (!quiet) puts("There is no data to export!"); //First day of the month?
        }
        else
        {
			FILE *csv;

			//Expand date specifiers in config::outputPath
			std::stringstream csvpath;
            csvpath << strftime_t(m_config.outputPath, inverters[0].monthData[0].datetime);
			CreatePath(csvpath.str().c_str());

            csvpath << FOLDER_SEP << m_config.plantname << "-" << strfgmtime_t("%Y%m", inverters[0].monthData[0].datetime) << ".csv";
			
			if ((csv = fopen(csvpath.str().c_str(), "w+")) == NULL)
			{
                if (m_config.quiet == 0)
				{
					snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
                    LOG_F(ERROR, "%s", msg);
				}
                return;
			}
			else
			{
                if (m_config.CSV_ExtendedHeader == 1)
				{
                    fprintf(csv, "sep=%c\n", m_config.delimiter);
                    fprintf(csv, "Version CSV1|Tool SBFspot%s (%s)|Linebreaks %s|Delimiter %s|Decimalpoint %s|Precision %d\n\n", m_config.prgVersion, OS, linebreak2txt(), delim2txt(m_config.delimiter), dp2txt(m_config.decimalpoint), m_config.precision);
                    for (const auto& inverter : inverters)
                        fprintf(csv, "%c%s%c%s", m_config.delimiter, inverter.DeviceName.c_str(), m_config.delimiter, inverter.DeviceName.c_str());
					fputs("\n", csv);
                    for (const auto& inverter : inverters)
                        fprintf(csv, "%c%s%c%s", m_config.delimiter, inverter.DeviceType.c_str(), m_config.delimiter, inverter.DeviceType.c_str());
					fputs("\n", csv);
                    for (const auto& inverter : inverters)
                        fprintf(csv, "%c%lu%c%lu", m_config.delimiter, inverter.serial, m_config.delimiter, inverter.serial);
					fputs("\n", csv);
                    for (const auto& inverter : inverters)
                        fprintf(csv, "%cTotal yield%cDay yield", m_config.delimiter, m_config.delimiter);
					fputs("\n", csv);
                    for (const auto& inverter : inverters)
                        fprintf(csv, "%cCounter%cAnalog", m_config.delimiter, m_config.delimiter);
					fputs("\n", csv);
				}
                if (m_config.CSV_Header == 1)
				{
                    char *DMY = DateTimeFormatToDMY(m_config.DateFormat);
					fprintf(csv, "%s", DMY);
					free(DMY);
                    for (const auto& inverter : inverters)
                        fprintf(csv, "%ckWh%ckWh", m_config.delimiter, m_config.delimiter);
					fputs("\n", csv);
				}
			}

			char FormattedFloat[16];

            for (unsigned int idx=0; idx<sizeof(inverters[0].monthData)/sizeof(MonthData); idx++)
			{
				time_t datetime = 0;
                for (const auto& inverter : inverters)
                    if (inverter.monthData[idx].datetime > 0)
                        datetime = inverter.monthData[idx].datetime;

				if (datetime > 0)
				{
                    fprintf(csv, "%s", strfgmtime_t(m_config.DateFormat, datetime));
                    for (const auto& inverter : inverters)
					{
                        fprintf(csv, "%c%s", m_config.delimiter, FormatDouble(FormattedFloat, (double)inverter.monthData[idx].totalWh/1000, 0, m_config.precision, m_config.decimalpoint));
                        fprintf(csv, "%c%s", m_config.delimiter, FormatDouble(FormattedFloat, (double)inverter.monthData[idx].dayWh/1000, 0, m_config.precision, m_config.decimalpoint));
					}
					fputs("\n", csv);
				}
			}
			fclose(csv);
		}
	}
}

void CsvExporter::exportDayData(const std::vector<InverterData>& inverters)
{
    char msg[80 + MAX_PATH];
	if (VERBOSE_NORMAL) puts("ExportDayDataToCSV()");

	FILE *csv;

	//fix 1.3.1 for inverters with BT piggyback (missing interval data in the dark)
	//need to find first valid date in array
	time_t date;
	unsigned int idx = 0;
	do
	{
        date = inverters[0].dayData[idx++].datetime;
    } while ((idx < sizeof(inverters[0].dayData)/sizeof(DayData)) && (date == 0));

	// Fix Issue 90: SBFspot still creating 1970 .csv files
    if (date == 0) return;	// Nothing to export! Silently exit.

	//Expand date specifiers in config::outputPath
	std::stringstream csvpath;
    csvpath << strftime_t(m_config.outputPath, date);
	CreatePath(csvpath.str().c_str());

    csvpath << FOLDER_SEP << m_config.plantname << "-" << strftime_t("%Y%m%d", date) << ".csv";

	if ((csv = fopen(csvpath.str().c_str(), "w+")) == NULL)
	{
        if (m_config.quiet == 0)
		{
			snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
            LOG_F(ERROR, "%s", msg);
		}
        return;
	}
	else
	{
        if (m_config.CSV_ExtendedHeader == 1)
		{
            fprintf(csv, "sep=%c\n", m_config.delimiter);
            fprintf(csv, "Version CSV1|Tool SBFspot%s (%s)|Linebreaks %s|Delimiter %s|Decimalpoint %s|Precision %d\n\n", m_config.prgVersion, OS, linebreak2txt(), delim2txt(m_config.delimiter), dp2txt(m_config.decimalpoint), m_config.precision);
            for (const auto& inverter : inverters)
                fprintf(csv, "%c%s%c%s", m_config.delimiter, inverter.DeviceName.c_str(), m_config.delimiter, inverter.DeviceName.c_str());
			fputs("\n", csv);
            for (const auto& inverter : inverters)
                fprintf(csv, "%c%s%c%s", m_config.delimiter, inverter.DeviceType.c_str(), m_config.delimiter, inverter.DeviceType.c_str());
			fputs("\n", csv);
            for (const auto& inverter : inverters)
                fprintf(csv, "%c%lu%c%lu", m_config.delimiter, inverter.serial, m_config.delimiter, inverter.serial);
			fputs("\n", csv);
            for (const auto& inverter : inverters)
                fprintf(csv, "%cTotal yield%cPower", m_config.delimiter, m_config.delimiter);
			fputs("\n", csv);
            for (const auto& inverter : inverters)
                fprintf(csv, "%cCounter%cAnalog", m_config.delimiter, m_config.delimiter);
			fputs("\n", csv);
		}
        if (m_config.CSV_Header == 1)
		{
            char *DMY = DateTimeFormatToDMY(m_config.DateTimeFormat);
			fputs(DMY, csv);
			free(DMY);
            for (const auto& inverter : inverters)
                fprintf(csv, "%ckWh%ckW", m_config.delimiter, m_config.delimiter);
			fputs("\n", csv);
		}
	}

	char FormattedFloat[16];

    for (unsigned int dd = 0; dd < sizeof(inverters[0].dayData)/sizeof(DayData); dd++)
	{
		time_t datetime = 0;
		unsigned long long totalPower = 0;
        for (const auto& inverter : inverters)
            if (inverter.dayData[dd].datetime > 0)
			{
                datetime = inverter.dayData[dd].datetime;
                totalPower += inverter.dayData[dd].watt;
			}

		if (datetime > 0)
		{
            if ((m_config.CSV_SaveZeroPower == 1) || (totalPower > 0))
			{
                fprintf(csv, "%s", strftime_t(m_config.DateTimeFormat, datetime));
                for (const auto& inverter : inverters)
				{
                    fprintf(csv, "%c%s", m_config.delimiter, FormatDouble(FormattedFloat, (double)inverter.dayData[dd].totalWh/1000, 0, m_config.precision, m_config.decimalpoint));
                    fprintf(csv, "%c%s", m_config.delimiter, FormatDouble(FormattedFloat, (double)inverter.dayData[dd].watt/1000, 0, m_config.precision, m_config.decimalpoint));
				}
				fputs("\n", csv);
			}
		}
	}
	fclose(csv);
}

int CsvExporter::WriteStandardHeader(FILE *csv, DEVICECLASS devclass)
{
	char *Header1, *Header2;

	char solar1[] = "||||Watt|Watt|Amp|Amp|Volt|Volt|Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|Watt|%|kWh|kWh|Hz|Hours|Hours|%|Status|Status|degC\n";
	char solar2[] = "|DeviceName|DeviceType|Serial|Pdc1|Pdc2|Idc1|Idc2|Udc1|Udc2|Pac1|Pac2|Pac3|Iac1|Iac2|Iac3|Uac1|Uac2|Uac3|PdcTot|PacTot|Efficiency|EToday|ETotal|Frequency|OperatingTime|FeedInTime|BT_Signal|Condition|GridRelay|Temperature\n";

	char batt1[] = "||||Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|kWh|kWh|Hz|hours|hours|Status|%|degC|Volt|Amp|Watt|Watt\n";
	char batt2[] = "|DeviceName|DeviceType|Serial|Pac1|Pac2|Pac3|Iac1|Iac2|Iac3|Uac1|Uac2|Uac3|PacTot|EToday|ETotal|Frequency|OperatingTime|FeedInTime|Condition|SOC|Tempbatt|Ubatt|Ibatt|TotWOut|TotWIn\n";

	if (devclass == BatteryInverter)
	{
		Header1 = batt1;
		Header2 = batt2;
	}
	else
	{
		Header1 = solar1;
		Header2 = solar2;
	}

    if (m_config.CSV_ExtendedHeader == 1)
	{
        fprintf(csv, "sep=%c\n", m_config.delimiter);
        fprintf(csv, "Version CSV1|Tool SBFspot%s (%s)|Linebreaks %s|Delimiter %s|Decimalpoint %s|Precision %d\n\n", m_config.prgVersion, OS, linebreak2txt(), delim2txt(m_config.delimiter), dp2txt(m_config.decimalpoint), m_config.precision);

		for (int i=0; Header1[i]!=0; i++)
            if (Header1[i]=='|') Header1[i]=m_config.delimiter;

		fputs(Header1, csv);
	}

    if (m_config.CSV_Header == 1)
	{
        char *DMY = DateTimeFormatToDMY(m_config.DateTimeFormat); //Caller must free the allocated memory
		fputs(DMY, csv);
		free(DMY);

		for (int i=0; Header2[i]!=0; i++)
            if (Header2[i]=='|') Header2[i]=m_config.delimiter;
		fputs(Header2, csv);
	}
	return 0;
}

int CsvExporter::WriteWebboxHeader(FILE *csv, const std::vector<InverterData>& inverters)
{	
	char *Header1, *Header2, *Header3;
	
	char solar1[] = "|DcMs.Watt[A]|DcMs.Watt[B]|DcMs.Amp[A]|DcMs.Amp[B]|DcMs.Vol[A]|DcMs.Vol[B]|GridMs.W.phsA|GridMs.W.phsB|GridMs.W.phsC|GridMs.A.phsA|GridMs.A.phsB|GridMs.A.phsC|GridMs.PhV.phsA|GridMs.PhV.phsB|GridMs.PhV.phsC|SS_PdcTot|GridMs.TotW|SBFspot.Efficiency|Metering.DykWh|Metering.TotWhOut|GridMs.Hz|Metering.TotOpTms|Metering.TotFeedTms|SBFspot.BTSignal|Operation.Health|Operation.GriSwStt|Coolsys.TmpNom";
	char solar2[] = "|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Counter|Counter|Analog|Counter|Counter|Analog|Counter|Counter|Analog|Status|Status|Analog";
	char solar3[] = "|Watt|Watt|Amp|Amp|Volt|Volt|Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|Watt|%|kWh|kWh|Hz|Hours|Hours|%|||degC";
	
	char batt1[] = "|GridMs.W.phsA|GridMs.W.phsB|GridMs.W.phsC|GridMs.A.phsA|GridMs.A.phsB|GridMs.A.phsC|GridMs.PhV.phsA|GridMs.PhV.phsB|GridMs.PhV.phsC|GridMs.TotW|Metering.DykWh|Metering.TotWhOut|GridMs.Hz|Metering.TotOpTms|Metering.TotFeedTms|Operation.Health|Bat.ChaStt|Bat.TmpVal|Bat.Vol|Bat.Amp|Metering.GridMs.TotWOut|Metering.GridMs.TotWIn";
	char batt2[] = "|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Analog|Counter|Counter|Analog|Counter|Counter|Status|Analog|Analog|Analog|Analog|Analog|Analog";
	char batt3[] = "|Watt|Watt|Watt|Amp|Amp|Amp|Volt|Volt|Volt|Watt|kWh|kWh|Hz|Hours|Hours||%|degC|Volt|Amp|Watt|Watt";

    if (inverters[0].DevClass == BatteryInverter)
	{
		Header1 = batt1;
		Header2 = batt2;
		Header3 = batt3;
	}
	else
	{
		Header1 = solar1;
		Header2 = solar2;
		Header3 = solar3;
	}

    if (m_config.CSV_ExtendedHeader == 1)
	{
        fprintf(csv, "sep=%c\n", m_config.delimiter);
        fprintf(csv, "Version CSV1|Tool SBFspot%s (%s)|Linebreaks %s|Delimiter %s|Decimalpoint %s|Precision %d\n\n", m_config.prgVersion, OS, linebreak2txt(), delim2txt(m_config.delimiter), dp2txt(m_config.decimalpoint), m_config.precision);

		int colcnt = 0;
		for (int i = 0; Header1[i] != 0; i++)
			if (Header1[i]=='|')
			{
                Header1[i]=m_config.delimiter;
				colcnt++;
			}

        for (const auto& inverter : inverters)
			for (int i = 0; i<colcnt; i++)
                fprintf(csv, "%c%s", m_config.delimiter, inverter.DeviceName.c_str());
		fputs("\n", csv);

        for (const auto& inverter : inverters)
			for (int i = 0; i < colcnt; i++)
                fprintf(csv, "%c%s", m_config.delimiter, inverter.DeviceType.c_str());
		fputs("\n", csv);

        for (const auto& inverter : inverters)
			for (int i = 0; i < colcnt; i++)
                fprintf(csv, "%c%lu", m_config.delimiter, inverter.serial);
		fputs("\n", csv);
	}

    if (m_config.CSV_Header == 1)
	{
		fputs("TimeStamp", csv);
        for (const auto& inverter : inverters)
			fputs(Header1, csv);
		fputs("\n", csv);
	}


    if (m_config.CSV_ExtendedHeader == 1)
	{
		for (int i=0; Header2[i]!=0; i++)
            if (Header2[i]=='|') Header2[i]=m_config.delimiter;

        for (const auto& inverter : inverters)
			fputs(Header2, csv);
		fputs("\n", csv);

        char *DMY = DateTimeFormatToDMY(m_config.DateTimeFormat); //Caller must free the allocated memory
		fputs(DMY, csv);
		free(DMY);

		for (int i=0; Header3[i]!=0; i++)
            if (Header3[i]=='|') Header3[i]=m_config.delimiter;
        for (const auto& inverter : inverters)
			fputs(Header3, csv);
		fputs("\n", csv);
	}
	return 0;
}

void CsvExporter::exportSpotData(std::time_t timestamp, const std::vector<InverterData>& inverters)
{
    // As from version 2.0.6 there is a new header for the spotdata.csv
    // It *should* be more compatible with SMA headers

	char msg[80 + MAX_PATH];
	if (VERBOSE_NORMAL) puts("ExportSpotDataToCSV()");

	FILE *csv;

	// Take time from computer instead of inverter
    time_t spottime = m_config.SpotTimeSource == 0 ? inverters[0].InverterDatetime : timestamp;

	//Expand date specifiers in config::outputPath
	std::stringstream csvpath;
    csvpath << strftime_t(m_config.outputPath, spottime);
	CreatePath(csvpath.str().c_str());

    csvpath << FOLDER_SEP << m_config.plantname << "-Spot-" << strftime_t("%Y%m%d", spottime) << ".csv";

	if ((csv = fopen(csvpath.str().c_str(), "a+")) == NULL)
	{
        if (m_config.quiet == 0)
		{
			snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
            LOG_F(ERROR, "%s", msg);
		}
        return;
	}
	else
	{
		//Write header when new file has been created
		#ifdef WIN32
		if (filelength(fileno(csv)) == 0)
		#else
		struct stat fStat;
		stat(csvpath.str().c_str(), &fStat);
		if (fStat.st_size == 0)
		#endif
		{
            if (m_config.SpotWebboxHeader == 0)
                WriteStandardHeader(csv, SolarInverter);
			else
                WriteWebboxHeader(csv, inverters);
		}

		char FormattedFloat[32];
		const char *strout = "%c%s";

        if (m_config.SpotWebboxHeader == 1)
            fputs(strftime_t(m_config.DateTimeFormat, spottime), csv);

        for (const auto& inverter : inverters)
		{
            if (m_config.SpotWebboxHeader == 0)
			{
                fputs(strftime_t(m_config.DateTimeFormat, spottime), csv);
                fprintf(csv, strout, m_config.delimiter, inverter.DeviceName.c_str());
                fprintf(csv, strout, m_config.delimiter, inverter.DeviceType.c_str());
                fprintf(csv, "%c%lu", m_config.delimiter, inverter.serial);
			}

            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Pdc1, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Pdc2, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Idc1/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Idc2/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Udc1/100, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Udc2/100, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Pac1, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Pac2, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Pac3, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Iac1/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Iac2/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Iac3/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Uac1/100, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Uac2/100, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Uac3/100, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.calPdcTot, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.TotalPac, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, inverter.calEfficiency, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatDouble(FormattedFloat, (double)inverter.EToday/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatDouble(FormattedFloat, (double)inverter.ETotal/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.GridFreq/100, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatDouble(FormattedFloat, (double)inverter.OperationTime/3600, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatDouble(FormattedFloat, (double)inverter.FeedInTime/3600, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatDouble(FormattedFloat, inverter.BT_Signal, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, tagdefs.getDesc(inverter.DeviceStatus, "?").c_str());
            fprintf(csv, strout, m_config.delimiter, tagdefs.getDesc(inverter.GridRelayStatus, "?").c_str());
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, (float)inverter.Temperature/100, 0, m_config.precision, m_config.decimalpoint));
            if (m_config.SpotWebboxHeader == 0)
				fputs("\n", csv);
		}

        if (m_config.SpotWebboxHeader == 1)
			fputs("\n", csv);

		fclose(csv);
	}
}

void CsvExporter::exportEventData(const std::vector<InverterData>& inverters, const std::string& dt_range_csv)
{
	char msg[80 + MAX_PATH];
	if (VERBOSE_NORMAL) puts("ExportEventsToCSV()");

	FILE *csv;

	//Expand date specifiers in config::outputPath_Events
	std::stringstream csvpath;
    csvpath << strftime_t(m_config.outputPath_Events, time(NULL));
	CreatePath(csvpath.str().c_str());

    csvpath << FOLDER_SEP << m_config.plantname << "-" << (m_config.smaUserGroup == UG_USER ? "User" : "Installer") << "-Events-" << dt_range_csv.c_str() << ".csv";

	if ((csv = fopen(csvpath.str().c_str(), "w+")) == NULL)
	{
        if (m_config.quiet == 0)
		{
			snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
            LOG_F(ERROR, "%s", msg);
		}
        return;
	}
	else
	{
		//Write header when new file has been created
		#ifdef WIN32
		if (filelength(fileno(csv)) == 0)
		#else
		struct stat fStat;
		stat(csvpath.str().c_str(), &fStat);
		if (fStat.st_size == 0)
		#endif
		{
            if (m_config.CSV_ExtendedHeader == 1)
			{
                fprintf(csv, "sep=%c\n", m_config.delimiter);
                fprintf(csv, "Version CSV1|Tool SBFspot%s (%s)|Linebreaks %s|Delimiter %s\n\n", m_config.prgVersion, OS, linebreak2txt(), delim2txt(m_config.delimiter));
			}
            if (m_config.CSV_Header == 1)
			{
				char Header[] = "DeviceType|DeviceLocation|SusyId|SerNo|TimeStamp|EntryId|EventCode|EventType|Category|Group|Tag|OldValue|NewValue|UserGroup\n";
				for (int i = 0; Header[i] != 0; i++)
					if (Header[i]=='|')
                        Header[i]=m_config.delimiter;
				fputs(Header, csv);
			}
		}

        for (auto& inverter : inverters)
		{
            auto eventData = inverter.eventData;
			// Sort events on ascending Entry_ID
            std::sort(eventData.begin(), eventData.end(), SortEntryID_Asc);

            for (auto it=eventData.begin(); it!=eventData.end(); ++it)
			{

                fprintf(csv, "%s%c", inverter.DeviceType.c_str(), m_config.delimiter);
                fprintf(csv, "%s%c", inverter.DeviceName.c_str(), m_config.delimiter);
                fprintf(csv, "%d%c", it->SUSyID(), m_config.delimiter);
                fprintf(csv, "%u%c", it->SerNo(), m_config.delimiter);
                fprintf(csv, "%s%c", strftime_t(m_config.DateTimeFormat, it->DateTime()), m_config.delimiter);
                fprintf(csv, "%d%c", it->EntryID(), m_config.delimiter);
                fprintf(csv, "%d%c", it->EventCode(), m_config.delimiter);
                fprintf(csv, "%s%c", it->EventType().c_str(), m_config.delimiter);
                fprintf(csv, "%s%c", it->EventCategory().c_str(), m_config.delimiter);
                fprintf(csv, "%s%c", tagdefs.getDesc(it->Group()).c_str(), m_config.delimiter);
				string EventDescription = tagdefs.getDesc(it->Tag());

				// If description contains "%s", replace it with localized parameter
				if (EventDescription.find("%s"))
					fprintf(csv, EventDescription.c_str(), tagdefs.getDescForLRI(it->Parameter()).c_str());
				else
					fprintf(csv, "%s", EventDescription.c_str());

                fprintf(csv, "%c", m_config.delimiter);

				// As an extra: export old and new values
				// This is "forgotten" in Sunny Explorer
				switch (it->DataType())
				{
				case 0x08: // Status
                    fprintf(csv, "%s%c", tagdefs.getDesc(it->OldVal() & 0xFFFF).c_str(), m_config.delimiter);
                    fprintf(csv, "%s%c", tagdefs.getDesc(it->NewVal() & 0xFFFF).c_str(), m_config.delimiter);
					break;

				case 0x00: // Unsigned int
                    fprintf(csv, "%u%c", it->OldVal(), m_config.delimiter);
                    fprintf(csv, "%u%c", it->NewVal(), m_config.delimiter);
					break;

				case 0x40: // Signed int
                    fprintf(csv, "%d%c", it->OldVal(), m_config.delimiter);
                    fprintf(csv, "%d%c", it->NewVal(), m_config.delimiter);
					break;

				case 0x10: // String
                    fprintf(csv, "%08X%c", it->OldVal(), m_config.delimiter);
                    fprintf(csv, "%08X%c", it->NewVal(), m_config.delimiter);
					break;

				default:
                    fprintf(csv, "%c%c", m_config.delimiter, m_config.delimiter);
				}

				// As an extra: User or Installer Event
				fprintf(csv, "%s\n", tagdefs.getDesc(it->UserGroupTagID()).c_str());
			}
		}

		fclose(csv);
	}
}

void CsvExporter::exportBatteryData(std::time_t timestamp, const std::vector<InverterData>& inverters)
{
	char msg[80 + MAX_PATH];
	if (VERBOSE_NORMAL) puts("ExportBatteryDataToCSV()");

	FILE *csv;

	//Expand date specifiers in config::outputPath
	std::stringstream csvpath;
    csvpath << strftime_t(m_config.outputPath, timestamp);
	CreatePath(csvpath.str().c_str());

    csvpath << FOLDER_SEP << m_config.plantname << "-Battery-" << strftime_t("%Y%m%d", timestamp) << ".csv";

	if ((csv = fopen(csvpath.str().c_str(), "a+")) == NULL)
	{
        if (m_config.quiet == 0)
		{
			snprintf(msg, sizeof(msg), "Unable to open output file %s\n", csvpath.str().c_str());
            LOG_F(ERROR, "%s", msg);
		}
        return;
	}
	else
	{
		//Write header when new file has been created
		#ifdef WIN32
		if (filelength(fileno(csv)) == 0)
		#else
		struct stat fStat;
		stat(csvpath.str().c_str(), &fStat);
		if (fStat.st_size == 0)
		#endif
		{
            if (m_config.SpotWebboxHeader == 0)
                WriteStandardHeader(csv, BatteryInverter);
			else
                WriteWebboxHeader(csv, inverters);
		}

		char FormattedFloat[32];
		const char *strout = "%c%s";

        if (m_config.SpotWebboxHeader == 1)
            fputs(strftime_t(m_config.DateTimeFormat, timestamp), csv);

        for (const auto& inverter : inverters)
		{
            if (m_config.SpotWebboxHeader == 0)
			{
                fputs(strftime_t(m_config.DateTimeFormat, timestamp), csv);
                fprintf(csv, strout, m_config.delimiter, inverter.DeviceName.c_str());
                fprintf(csv, strout, m_config.delimiter, inverter.DeviceType.c_str());
                fprintf(csv, "%c%lu", m_config.delimiter, inverter.serial);
			}

            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.Pac1), 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.Pac2), 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.Pac3), 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.Uac1)/100, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.Uac2)/100, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.Uac3)/100, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.Iac1)/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.Iac2)/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.Iac3)/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.TotalPac), 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatDouble(FormattedFloat, ((double)inverter.EToday)/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatDouble(FormattedFloat, ((double)inverter.ETotal)/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatDouble(FormattedFloat, ((double)inverter.GridFreq)/100, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatDouble(FormattedFloat, ((double)inverter.OperationTime)/3600, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.FeedInTime)/3600, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, tagdefs.getDesc(inverter.DeviceStatus, "?").c_str());
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.BatChaStt), 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.BatTmpVal)/10, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.BatVol)/100, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.BatAmp)/1000, 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.MeteringGridMsTotWOut), 0, m_config.precision, m_config.decimalpoint));
            fprintf(csv, strout, m_config.delimiter, FormatFloat(FormattedFloat, ((float)inverter.MeteringGridMsTotWIn), 0, m_config.precision, m_config.decimalpoint));
            if (m_config.SpotWebboxHeader == 0)
				fputs("\n", csv);
		}
        if (m_config.SpotWebboxHeader == 1)
			fputs("\n", csv);

		fclose(csv);
	}
}

// Undocumented - For WebSolarLog usage only
// WSL needs unlocalized strings
const char *CsvExporter::WSL_AttributeToText(int attribute)
{
	switch (attribute)
	{
		//Grid Relay Status
		case 311: return "Open";
		case 51: return "Closed";

		//Device Status
		case 307: return "OK";
		case 455: return "Warning";
		case 35: return "Fault";

		default: return "?";
	}
}

//Undocumented - For WebSolarLog usage only
//TODO:
//Version 2.1.0 (multi inverter plant) takes only values from first inverter
//Must be calculated for ALL inverters - Align with WSL developers
void CsvExporter::ExportSpotDataToWSL(const std::vector<InverterData>& inverters)
{
	if (VERBOSE_NORMAL) puts("ExportSpotDataToWSL()");

    time_t spottime = inverters[0].InverterDatetime;
    if (m_config.SpotTimeSource == 1) // Take time from computer instead of inverter
		time(&spottime);


	char FormattedFloat[32];
	const char *strout = "%s%c";

    printf(strout,"WSL_START", m_config.delimiter);
    printf(strout, strftime_t(m_config.DateTimeFormat, spottime), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Pdc1, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Pdc2, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Idc1/1000, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Idc2/1000, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Udc1/100, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Udc2/100, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Pac1, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Pac2, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Pac3, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Iac1/1000, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Iac2/1000, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Iac3/1000, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Uac1/100, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Uac2/100, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].Uac3/100, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].calPdcTot, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].TotalPac, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, inverters[0].calEfficiency, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatDouble(FormattedFloat, (double)inverters[0].EToday/1000, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatDouble(FormattedFloat, (double)inverters[0].ETotal/1000, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatFloat(FormattedFloat, (float)inverters[0].GridFreq/100, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatDouble(FormattedFloat, (double)inverters[0].OperationTime/3600, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatDouble(FormattedFloat, (double)inverters[0].FeedInTime/3600, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, FormatDouble(FormattedFloat, inverters[0].BT_Signal, 0, m_config.precision, m_config.decimalpoint), m_config.delimiter);
    printf(strout, tagdefs.getDesc(inverters[0].DeviceStatus, "?").c_str(), m_config.delimiter);
    printf(strout, tagdefs.getDesc(inverters[0].GridRelayStatus, "?").c_str(), m_config.delimiter);
	printf("%s\n", "WSL_END");
}

//Undocumented - For 123Solar Web Solar logger usage only)
void CsvExporter::ExportSpotDataTo123s(const std::vector<InverterData>& inverters)
{
	if (VERBOSE_NORMAL) puts("ExportSpotDataTo123s()");

	// Currently, only data of first inverter is exported
    const InverterData& invdata = inverters[0];

	char FormattedFloat[32];
	const char *strout = "%s%c";
	const char *s123_dt_format = "%Y%m%d-%H:%M:%S";
	const char *s123_delimiter = " ";
	const char *s123_decimalpoint = ".";

	//Calculated DC Power Values (Sum of Power per string)
    float calPdcTot = (float)(invdata.Pdc1 + invdata.Pdc2);

	//Calculated AC Side Values (Sum of Power & Current / Maximum of Voltage)
    float calPacTot = (float)(invdata.Pac1 + invdata.Pac2 + invdata.Pac3);
    float calIacTot = (float)(invdata.Iac1 + invdata.Iac2 + invdata.Iac3);
    float calUacMax = (float)max(max(invdata.Uac1, invdata.Uac2), invdata.Uac3);

	//Calculated Inverter Efficiency
	float calEfficiency = calPdcTot == 0 ? 0 : calPacTot / calPdcTot * 100;

	//Select Between Computer & Inverter Time (As Per .cfg Setting)
    time_t spottime = invdata.InverterDatetime;
    if (m_config.SpotTimeSource == 1) // Take time from computer instead of inverter
		time(&spottime);

	//Send Spot Data Frame to 123Solar

	// $SDTE = Date & Time ( YYYYMMDD-HH:MM:SS )
	printf(strout, strftime_t(s123_dt_format, spottime), *s123_delimiter);
	// $G1V  = GridMs.PhV.phsA
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Uac1/100, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $G1A  = GridMs.A.phsA
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Iac1/1000, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $G1P  = GridMs.W.phsA
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Pac1, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $G2V  = GridMs.PhV.phsB
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Uac2/100, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $G2A  = GridMs.A.phsB
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Iac2/1000, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $G2P  = GridMs.W.phsB
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Pac2, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $G3V  = GridMs.PhV.phsC
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Uac3/100, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $G3A  = GridMs.A.phsC
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Iac3/1000, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $G3P  = GridMs.W.phsC
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Pac3, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $FRQ = GridMs.Hz
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.GridFreq/100, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $EFF = Value computed by SBFspot
    printf(strout, FormatFloat(FormattedFloat, calEfficiency, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $INVT = Inverter temperature 
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Temperature/100, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $BOOT = Booster temperature - n/a for SMA inverters
    printf(strout, FormatFloat(FormattedFloat, 0., 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $KWHT = Metering.TotWhOut (kWh)
    printf(strout, FormatDouble(FormattedFloat, (double)invdata.ETotal/1000, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $I1V = DcMs.Vol[A]
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Udc1/100, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $I1A = DcMs.Amp[A]
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Idc1/1000, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $I1P = DcMs.Watt[A]
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Pdc1, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $I2V = DcMs.Vol[B]
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Udc2/100, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $I2A = DcMs.Amp[B]
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Idc2/1000, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $I2P = DcMs.Watt[B]
    printf(strout, FormatFloat(FormattedFloat, (float)invdata.Pdc2, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $GV = Was grid voltage in single phase 123Solar - For backwards compatibility
    printf(strout, FormatFloat(FormattedFloat, (float)calUacMax/100, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $GA = Was grid current in single phase 123Solar - For backwards compatibility
    printf(strout, FormatFloat(FormattedFloat, (float)calIacTot/1000, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// $GP = Was grid power in single phase 123Solar - For backwards compatibility
    printf(strout, FormatFloat(FormattedFloat, (float)calPacTot, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// OK
	printf("%s\n", ">>>S123:OK");
}

//Undocumented - For 123Solar Web Solar logger usage only)
void CsvExporter::ExportInformationDataTo123s(const std::vector<InverterData>& inverters)
{
	if (VERBOSE_NORMAL) puts("ExportInformationDataTo123s()");

	// Currently, only data of first inverter is exported
    const InverterData& invdata = inverters[0];

	const char *s123_dt_format = "%Y%m%d-%H:%M:%S";
	const char *s123_delimiter = "\n";

	//Send Inverter Information File to 123Solar

	// Reader program name & version
    printf("Reader: SBFspot %s%c", m_config.prgVersion, *s123_delimiter);
	// Inverter bluetooth address
    printf("Bluetooth Address: %s%c", m_config.BT_Address, *s123_delimiter);
	// Inverter bluetooth NetID
    printf("Bluetooth NetID: %02X%c", invdata.NetID, *s123_delimiter);
	// Inverter time ( YYYYMMDD-HH:MM:SS )
    printf("Time: %s%c", strftime_t(s123_dt_format, invdata.InverterDatetime), *s123_delimiter);
	// Inverter device name
    printf("Device Name: %s%c", invdata.DeviceName.c_str(), *s123_delimiter);
	// Inverter device class
    printf("Device Class: %s%c", invdata.DeviceClass.c_str(), *s123_delimiter);
	// Inverter device type
    printf("Device Type: %s%c", invdata.DeviceType.c_str(), *s123_delimiter);
	// Inverter serial number
    printf("serial Number: %lu%c", invdata.serial, *s123_delimiter);
	// Inverter firmware version
    printf("Firmware: %s%c", invdata.SWVersion.c_str(), *s123_delimiter);
	// Inverter phase maximum Pac
    if (invdata.Pmax1 > 0) printf("Pac Max Phase 1: %luW%c", invdata.Pmax1, *s123_delimiter);
    if (invdata.Pmax2 > 0) printf("Pac Max Phase 2: %luW%c", invdata.Pmax2, *s123_delimiter);
    if (invdata.Pmax3 > 0) printf("Pac Max Phase 3: %luW%c", invdata.Pmax3, *s123_delimiter);
	// Inverter wake-up & sleep times
    printf("Wake-Up Time: %s%c", strftime_t(s123_dt_format, invdata.WakeupTime), *s123_delimiter);
    printf("Sleep Time: %s\n", strftime_t(s123_dt_format, invdata.SleepTime));
}

//Undocumented - For 123Solar Web Solar logger usage only)
void CsvExporter::ExportStateDataTo123s(const std::vector<InverterData>& inverters)
{
	if (VERBOSE_NORMAL) puts("ExportStateDataTo123s()");

	// Currently, only data of first inverter is exported
    const InverterData& invdata = inverters[0];

	char FormattedFloat[32];
	const char *s123_dt_format = "%Y%m%d-%H:%M:%S";
	const char *s123_delimiter = "\n";
	const char *s123_decimalpoint = ".";

	//Send Inverter State Data to 123Solar

	// Inverter time ( YYYYMMDD-HH:MM:SS )
    printf("Inverter Time: %s%c", strftime_t(s123_dt_format, invdata.InverterDatetime), *s123_delimiter);
	// Inverter device name
    printf("Device Name: %s%c", invdata.DeviceName.c_str(), *s123_delimiter);
	// Device status
    printf("Device Status: %s%c", tagdefs.getDesc(invdata.DeviceStatus, "?").c_str(), *s123_delimiter);
	// Grid relay status
    printf("GridRelay Status: %s%c", tagdefs.getDesc(invdata.GridRelayStatus, "?").c_str(), *s123_delimiter);
	// Operation time
    printf("Operation Time: %s%c", FormatDouble(FormattedFloat, (double)invdata.OperationTime/3600, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// Feed-in time
    printf("Feed-In Time: %s%c", FormatDouble(FormattedFloat, (double)invdata.FeedInTime/3600, 0, m_config.precision, *s123_decimalpoint), *s123_delimiter);
	// Bluetooth signal
    printf("Bluetooth Signal: %s\n", FormatDouble(FormattedFloat, invdata.BT_Signal, 0, m_config.precision, *s123_decimalpoint));
}
