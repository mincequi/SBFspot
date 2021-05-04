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

#include "SmaInverter.h"

#include <QNetworkDatagram>
#include <QtConcurrent>

#include <SpeedwireHeader.hpp>
#include <SpeedwireInverterProtocol.hpp>

#include <Config.h>
#include <Defines.h>
#include <Ethernet_qt.h>
#include <Exporter.h>
#include <Logger.h>
#include <SBFspot.h>
#include <Storage.h>
#include <misc.h>
#include <sma/SmaInverterRequests.h>

namespace sma {

template <class T>
void setClsData(std::vector<ElectricParameters>& data, uint8_t cls, T value, T ElectricParameters::*field ) {
    if (cls < 1) {
        return;
    }

    if (data.size() < cls) {
        data.resize(cls);
    }

    data.at(cls-1).*field = value;
}

SmaInverter::SmaInverter(QObject* parent, const Config& config, Ethernet_qt& ioDevice, uint32_t address, Storage* storage) :
    QObject(parent),
    m_config(config),
    m_ioDevice(ioDevice),
    m_storage(storage),
    m_address(address),
    m_pendingLiveData(0) {
    resetPendingData();
    init();
}

void SmaInverter::init() {
    // QtConcurrent::run([&](){
        auto buffer = m_sbfSpot.encodeInitRequest();
        // for (auto i = 0; (i < 5) && (m_state == State::Invalid); ++i) {
            m_ioDevice.send(buffer, m_address, 9522);
            // QThread::sleep(1);
        // }
    //});
}

void SmaInverter::login(std::time_t timestamp) {
    if (m_state == State::Invalid) {
        LOG_S(WARNING) << "(" << m_serial << ") Inverter not yet initialized";
        return;
    }

    LOG_S(1) << "(" << m_serial << ") Logging in to inverter";

    m_pendingLiveData.timestamp = timestamp;

    // QtConcurrent::run([&](){
        auto buffer = m_sbfSpot.encodeLoginRequest(m_config.smaUserGroup, std::string(m_config.smaPassword));
        // for (auto i = 0; (i < 5) && (m_state == State::Initialized); ++i) {
            m_ioDevice.send(buffer, m_address, 9522);
            // QThread::sleep(1);
        // }
    // });
}

void SmaInverter::logout() {
    if (m_state != State::LoggedIn) {
        LOG_S(WARNING) << "(" << m_serial << ") Not logged in";
        return;
    }

    auto buffer = m_sbfSpot.encodeLogoutRequest();
    m_ioDevice.send(buffer, m_address, 9522);
    m_state = State::Initialized;
    emit stateChanged(m_state);
}

void SmaInverter::requestLiveData() {
    if (m_state != State::LoggedIn) {
        LOG_S(WARNING) << "(" << m_serial << ") Not logged in";
        return;
    }

    requestDataSet(SoftwareVersion);
    requestDataSet(TypeLabel);
    requestDataSet(DeviceStatus);
    // Temperature is not answered by all inverter models. So, removing it.
    //requestDataSet(InverterTemperature);
    requestDataSet(MaxACPower);
    requestDataSet(EnergyProduction);
    requestDataSet(OperationTime);
    requestDataSet(SpotDCPower);
    requestDataSet(SpotDCVoltage);
    requestDataSet(SpotACPower);
    requestDataSet(SpotACVoltage);
    requestDataSet(SpotACTotalPower);
    requestDataSet(SpotGridFrequency);
    //requestDataSet(MeteringGridMsTotW);

    if (!m_storage) {
        return;
    }

    if (m_pendingLiveData.timestamp % m_config.archiveInterval == m_config.liveInterval) {
        auto missingSequence = m_storage->nextMissingDayData(m_pendingLiveData.timestamp, m_serial);
        if (missingSequence.to != 0) {
            requestDayData(missingSequence.from, missingSequence.to);
        }

        missingSequence = m_storage->nextMissingMonthData(m_pendingLiveData.timestamp, m_serial);
        if (missingSequence.to != 0) {
            requestMonthData(missingSequence.from, missingSequence.to);
        }
    }
}

void SmaInverter::requestDayData(std::time_t from, std::time_t to) {
    if (m_state != State::LoggedIn) {
        LOG_S(WARNING) << "(" << m_serial << ") Not logged in";
        return;
    }

    LOG_S(INFO) << "(" << m_serial << ") Requesting day data from: " << from << ", to: " << to;
    auto buffer = m_sbfSpot.encodeHistoricDayDataRequest(m_susyId, m_serial, from, to, BluetoothAddress());
    m_ioDevice.send(buffer, m_address, 9522);
}

void SmaInverter::requestMonthData(std::time_t from, std::time_t to) {
    if (m_state != State::LoggedIn) {
        LOG_S(WARNING) << "(" << m_serial << ") Not logged in";
        return;
    }

    LOG_S(INFO) << "(" << m_serial << ") Requesting month data from: " << from << ", to: " << to;
    auto buffer = m_sbfSpot.encodeHistoricMonthDataRequest(m_susyId, m_serial, from, to, BluetoothAddress());
    m_ioDevice.send(buffer, m_address, 9522);
}

std::list<SmaResponse> SmaInverter::result() {
    std::stringstream ss;
    ss << std::uppercase << std::hex;
    for (const auto& lri: m_pendingLris) {
        ss << lri << ", ";
    }

    LOG_IF_S(WARNING, !m_pendingLris.empty()) << "Polling timed out. Discarding requests: " << ss.str();

    m_pendingLiveData.fixup();

    std::list<SmaResponse> result;
    result.push_back(m_pendingLiveData);
    if (!m_pendingDayData.empty())
        result.push_back(m_pendingDayData);
    if (!m_pendingMonthData.empty())
        result.push_back(m_pendingMonthData);
    resetPendingData();

    logout();

    return result;
}

void SmaInverter::resetPendingData() {
    m_pendingLris.clear();
    m_pendingLiveData = LiveData(m_serial);
    m_pendingDayData.clear();
    m_pendingMonthData.clear();
    //m_pendingData.ac.resize(3);
    m_pendingDataMap.clear();
}

void SmaInverter::requestDataSet(SmaInverterDataSet dataSet) {
    auto lri = static_cast<LriDef>(SmaInverterRequests::create(dataSet).first);
    if (lri == 0) {
        LOG_S(WARNING) << "Illegal LRI";
    } else {
        m_pendingLris.insert(lri);
    }

    auto buffer = m_sbfSpot.encodeDataRequest(m_susyId, m_serial, dataSet);
    m_ioDevice.send(buffer, m_address, 9522);
}

void SmaInverter::onDatagram(const QNetworkDatagram& datagram) {
    auto ba = datagram.data();
    ByteBuffer buffer(ba.begin(), ba.end());
    const char* data = datagram.data().data() + sizeof(ethPacketHeaderL1) - 1;
    ethPacket *pckt = (ethPacket*)data;
    switch (m_state) {
    case State::Invalid:
        m_susyId = pckt->Source.SUSyID;	// Fix Issue 98
        m_serial = pckt->Source.serial;	// Fix Issue 98
        LOG_S(INFO) << "Inverter " << datagram.senderAddress().toString().toStdString() << ", serial: " << m_serial;
        resetPendingData();
        m_state = State::Initialized;
        emit stateChanged(m_state);
        return;
    case State::Initialized:
        if (btohs(pckt->ErrorCode) == 0) {
            m_state = State::LoggedIn;
            emit stateChanged(m_state);
        } else {
            LOG_S(ERROR) << "Login error. Password correct?";
        }
        return;
    case State::LoggedIn:
        decodeResponse(buffer, m_pendingDataMap, m_pendingLris);
        break;
    }
}

void SmaInverter::decodeResponse(ByteBuffer& buffer, InverterDataMap& inverterDataMap, std::set<LriDef>& lris) {
    auto packet = SmaPacket::fromBuffer(buffer);
    if (packet.payload.size() < 12) {
        LOG_F(WARNING, "(%u) Invalid payload size: %lu, for packet type: %X", m_serial, packet.payload.size(), packet.dataSet);
        return;
    }

    switch (packet.dataSet) {
    case HistoricDayDataResponse:
        return decodeDayData(packet.payload);
        break;
    case HistoricMonthDataResponse:
        return decodeMonthData(packet.payload);
        break;
    default:
        break;
    }

    int32_t value = 0;
    int64_t value64 = 0;
    unsigned char Vtype = 0;
    unsigned char Vbuild = 0;
    unsigned char Vminor = 0;
    unsigned char Vmajor = 0;

    int recordsize = 0;
    for (uint ii = 40 + sizeof(ethPacketHeaderL1); ii < buffer.size() - 3; ii += recordsize) {
        uint32_t code = ((uint32_t)get_long(buffer.data() + ii));
        LriDef lri = (LriDef)(code & 0x00FFFF00);
        uint32_t cls = code & 0xFF;
        unsigned char dataType = code >> 24;
        time_t datetime = (time_t)get_long(buffer.data() + ii + 4);

        // fix: We can't rely on dataType because it can be both 0x00 or 0x40 for DWORDs
        if ((lri == MeteringDyWhOut) || (lri == MeteringTotWhOut) || (lri == MeteringTotFeedTms) || (lri == MeteringTotOpTms))	//QWORD
        //if ((code == SPOT_ETODAY) || (code == SPOT_ETOTAL) || (code == SPOT_FEEDTM) || (code == SPOT_OPERTM))	//QWORD
        {
            value64 = get_longlong(buffer.data() + ii + 8);
            if ((value64 == (int64_t)NaN_S64) || (value64 == (int64_t)NaN_U64)) value64 = 0;
            inverterDataMap[lri] = value64;
        }
        else if ((lri >= OperationHealth) && (lri <= InverterWLim) &&
                 (dataType != 0x10) && (dataType != 0x08))	//Not TEXT or STATUS, so it should be DWORD
        {
            value = (int32_t)get_long(buffer.data() + ii + 16);
            if ((value == (int32_t)NaN_S32) || (value == (int32_t)NaN_U32)) value = 0;
            inverterDataMap[lri] = value;
        }

        switch (lri)
        {
        case GridMsTotW: //SPOT_PACTOT
            if (recordsize == 0) recordsize = 28;
            //This function gives us the time when the inverter was switched off
            //inverter.SleepTime = datetime;
            m_pendingLiveData.acPowerTotal = value;
            // inverter.flags |= type;
            break;

        case OperationHealthSttOk: //INV_PACMAX1
            if (recordsize == 0) recordsize = 28;
            //inverter.Pmax1 = value;
            // inverter.flags |= type;
            break;

        case OperationHealthSttWrn: //INV_PACMAX2
            if (recordsize == 0) recordsize = 28;
            //inverter.Pmax2 = value;
            // inverter.flags |= type;
            break;

        case OperationHealthSttAlm: //INV_PACMAX3
            if (recordsize == 0) recordsize = 28;
            //inverter.Pmax3 = value;
            // inverter.flags |= type;
            break;

        case GridMsWphsA: //SPOT_PAC1
            if (recordsize == 0) recordsize = 28;
            m_pendingLiveData.ac[0].power = value;
            // inverter.flags |= type;
            break;

        case GridMsWphsB: //SPOT_PAC2
            if (recordsize == 0) recordsize = 28;
            m_pendingLiveData.ac[1].power = value;
            // inverter.flags |= type;
            break;

        case GridMsWphsC: //SPOT_PAC3
            if (recordsize == 0) recordsize = 28;
            m_pendingLiveData.ac[2].power = value;
            // inverter.flags |= type;
            break;

        case GridMsPhVphsA: //SPOT_UAC1
            if (recordsize == 0) recordsize = 28;
            m_pendingLiveData.ac[0].voltage = value/100.0f;
            // inverter.flags |= type;
            break;

        case GridMsPhVphsB: //SPOT_UAC2
            if (recordsize == 0) recordsize = 28;
            m_pendingLiveData.ac[1].voltage = value/100.0f;
            // inverter.flags |= type;
            break;

        case GridMsPhVphsC: //SPOT_UAC3
            if (recordsize == 0) recordsize = 28;
            m_pendingLiveData.ac[2].voltage = value/100.0f;
            // inverter.flags |= type;
            break;

        case GridMsAphsA_1: //SPOT_IAC1
        case GridMsAphsA:
            if (recordsize == 0) recordsize = 28;
            m_pendingLiveData.ac[0].current = value/1000.0f;
            // inverter.flags |= type;
            break;

        case GridMsAphsB_1: //SPOT_IAC2
        case GridMsAphsB:
            if (recordsize == 0) recordsize = 28;
            m_pendingLiveData.ac[1].current = value/1000.0f;
            // inverter.flags |= type;
            break;

        case GridMsAphsC_1: //SPOT_IAC3
        case GridMsAphsC:
            if (recordsize == 0) recordsize = 28;
            m_pendingLiveData.ac[2].current = value/1000.0f;
            // inverter.flags |= type;
            break;

        case GridMsHz: //SPOT_FREQ
            if (recordsize == 0) recordsize = 28;
            //inverter.GridFreq = value;
            // inverter.flags |= type;
            break;

        case DcMsWatt: //SPOT_PDC1 / SPOT_PDC2
            if (recordsize == 0) recordsize = 28;
            setClsData(m_pendingLiveData.dc, cls, value, &ElectricParameters::power);
            break;

        case DcMsVol: //SPOT_UDC1 / SPOT_UDC2
            if (recordsize == 0) recordsize = 28;
            setClsData(m_pendingLiveData.dc, cls, value/100.0f, &ElectricParameters::voltage);
            break;

        case DcMsAmp: //SPOT_IDC1 / SPOT_IDC2
            if (recordsize == 0) recordsize = 28;
            setClsData(m_pendingLiveData.dc, cls, value/1000.0f, &ElectricParameters::current);
            break;

        case MeteringTotWhOut: //SPOT_ETOTAL
            if (recordsize == 0) recordsize = 16;
            //In case SPOT_ETODAY missing, this function gives us inverter time (eg: SUNNY TRIPOWER 6.0)
            //inverter.InverterDatetime = datetime;
            m_pendingLiveData.energyExportTotal = value64;
            // inverter.flags |= type;
            break;

        case MeteringDyWhOut: //SPOT_ETODAY
            if (recordsize == 0) recordsize = 16;
            //This function gives us the current inverter time
            //inverter.InverterDatetime = datetime;
            m_pendingLiveData.energyExportToday = value64;
            // inverter.flags |= type;
            break;

        case MeteringTotOpTms: //SPOT_OPERTM
            if (recordsize == 0) recordsize = 16;
            //inverter.OperationTime = value64;
            // inverter.flags |= type;
            break;

        case MeteringTotFeedTms: //SPOT_FEEDTM
            if (recordsize == 0) recordsize = 16;
            //inverter.FeedInTime = value64;
            // inverter.flags |= type;
            break;

        case NameplateLocation: //INV_NAME
            if (recordsize == 0) recordsize = 40;
            //This function gives us the time when the inverter was switched on
            //inverter.WakeupTime = datetime;
            //char deviceName[33];
            //strncpy(deviceName, (char *)buffer.data() + ii + 8, sizeof(deviceName)-1);
            //inverter.DeviceName = deviceName;
            //// inverter.flags |= type;
            //if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_NAME", inverter.DeviceName.c_str(), ctime(&datetime));
            break;

        case NameplatePkgRev: //INV_SWVER
            if (recordsize == 0) recordsize = 40;
            //Vtype = buffer[ii + 24];
            //char ReleaseType[4];
            //if (Vtype > 5)
            //    sprintf(ReleaseType, "%d", Vtype);
            //else
            //    sprintf(ReleaseType, "%c", "NEABRS"[Vtype]); //NOREV-EXPERIMENTAL-ALPHA-BETA-RELEASE-SPECIAL
            //Vbuild = buffer[ii + 25];
            //Vminor = buffer[ii + 26];
            //Vmajor = buffer[ii + 27];
            ////Vmajor and Vminor = 0x12 should be printed as '12' and not '18' (BCD)
            //char swVersion[16];
            //snprintf(swVersion, sizeof(swVersion), "%c%c.%c%c.%02d.%s", '0'+(Vmajor >> 4), '0'+(Vmajor & 0x0F), '0'+(Vminor >> 4), '0'+(Vminor & 0x0F), Vbuild, ReleaseType);
            //inverter.SWVersion = swVersion;
            //// inverter.flags |= type;
            //if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_SWVER", inverter.SWVersion.c_str(), ctime(&datetime));
            break;

        case NameplateModel: //INV_TYPE
            if (recordsize == 0) recordsize = 40;
            //for (int idx = 8; idx < recordsize; idx += 4)
            //{
            //    unsigned long attribute = ((unsigned long)get_long(buffer.data() + ii + idx)) & 0x00FFFFFF;
            //    unsigned char status = buffer[ii + idx + 3];
            //    if (attribute == 0xFFFFFE) break;	//End of attributes
            //    if (status == 1)
            //    {
            //        std::string devtype = tagdefs.getDesc(attribute);
            //        if (!devtype.empty())
            //        {
            //            inverter.DeviceType = devtype;
            //        }
            //        else
            //        {
            //            inverter.DeviceType = "UNKNOWN TYPE";
            //            printf("Unknown Inverter Type. Report this issue at https://github.com/SBFspot/SBFspot/issues with following info:\n");
            //            printf("0x%08lX and Inverter Type=<Fill in the exact type> (e.g. SB1300TL-10)\n", attribute);
            //        }
            //    }
            //}
            // inverter.flags |= type;
            break;

        case NameplateMainModel: //INV_CLASS
            if (recordsize == 0) recordsize = 40;
            //for (int idx = 8; idx < recordsize; idx += 4)
            //{
            //    unsigned long attribute = ((unsigned long)get_long(buffer.data() + ii + idx)) & 0x00FFFFFF;
            //    unsigned char attValue = buffer[ii + idx + 3];
            //    if (attribute == 0xFFFFFE) break;	//End of attributes
            //    if (attValue == 1)
            //    {
            //        inverter.DevClass = (DEVICECLASS)attribute;
            //        std::string devclass = tagdefs.getDesc(attribute);
            //        if (!devclass.empty())
            //        {
            //            inverter.DeviceClass = devclass;
            //        }
            //        else
            //        {
            //            inverter.DeviceClass = "UNKNOWN CLASS";
            //            printf("Unknown Device Class. Report this issue at https://github.com/SBFspot/SBFspot/issues with following info:\n");
            //            printf("0x%08lX and Device Class=...\n", attribute);
            //        }
            //    }
            //}
            // inverter.flags |= type;
            break;

        case OperationHealth: //INV_STATUS:
            if (recordsize == 0) recordsize = 40;
            //for (int idx = 8; idx < recordsize; idx += 4)
            //{
            //    unsigned long attribute = ((unsigned long)get_long(buffer.data() + ii + idx)) & 0x00FFFFFF;
            //    unsigned char attValue = buffer[ii + idx + 3];
            //    if (attribute == 0xFFFFFE) break;	//End of attributes
            //    if (attValue == 1)
            //        inverter.DeviceStatus = attribute;
            //}
            // inverter.flags |= type;
            break;

        case OperationGriSwStt: //INV_GRIDRELAY
            if (recordsize == 0) recordsize = 40;
            //for (int idx = 8; idx < recordsize; idx += 4)
            //{
            //    unsigned long attribute = ((unsigned long)get_long(buffer.data() + ii + idx)) & 0x00FFFFFF;
            //    unsigned char attValue = buffer[ii + idx + 3];
            //    if (attribute == 0xFFFFFE) break;	//End of attributes
            //    if (attValue == 1)
            //        inverter.GridRelayStatus = attribute;
            //}
            // inverter.flags |= type;
            break;

        case BatChaStt:
            if (recordsize == 0) recordsize = 28;
            //inverter.BatChaStt = value;
            // inverter.flags |= type;
            break;

        case BatDiagCapacThrpCnt:
            if (recordsize == 0) recordsize = 28;
            //inverter.BatDiagCapacThrpCnt = value;
            // inverter.flags |= type;
            break;

        case BatDiagTotAhIn:
            if (recordsize == 0) recordsize = 28;
            //inverter.BatDiagTotAhIn = value;
            // inverter.flags |= type;
            break;

        case BatDiagTotAhOut:
            if (recordsize == 0) recordsize = 28;
            //inverter.BatDiagTotAhOut = value;
            // inverter.flags |= type;
            break;

        case BatTmpVal:
            if (recordsize == 0) recordsize = 28;
            //inverter.BatTmpVal = value;
            // inverter.flags |= type;
            break;

        case BatVol:
            if (recordsize == 0) recordsize = 28;
            //inverter.BatVol = value;
            // inverter.flags |= type;
            break;

        case BatAmp:
            if (recordsize == 0) recordsize = 28;
            //inverter.BatAmp = value;
            // inverter.flags |= type;
            break;

        case CoolsysTmpNom:
            if (recordsize == 0) recordsize = 28;
            //inverter.Temperature = value;
            // inverter.flags |= type;
            break;

        case MeteringGridMsTotWOut:
            if (recordsize == 0) recordsize = 28;
            //inverter.MeteringGridMsTotWOut = value;
            break;

        case MeteringGridMsTotWIn:
            if (recordsize == 0) recordsize = 28;
            //inverter.MeteringGridMsTotWIn = value;
            break;

        default:
            if (recordsize == 0) recordsize = 12;
            //return decodeDayData(buffer);
        } // switch (lri)

        lris.erase(lri);
    }
}

void SmaInverter::decodeDayData(const ByteBuffer& buffer) {
    const uint recordsize = 12;
    m_pendingDayData.reserve(buffer.size()/recordsize);

    std::time_t endOfDayData = 0;
    for (uint x = 0; x < (buffer.size() - recordsize); x += recordsize) {
        auto datetime = (time_t)get_long(buffer.data() + x);
        auto totalWh = (unsigned long long)get_longlong(buffer.data() + x + 4);
        if (totalWh == NaN_U64) {
            endOfDayData = std::max(endOfDayData, datetime);
            continue;   // Fix Issue 109: Bad request 400: Power value too high for system size
        }

        DayData dayData;
        dayData.datetime = datetime;
        dayData.totalWh = totalWh;
        dayData.serial = m_serial;
        LOG_S(1) << dayData;
        m_pendingDayData.push_back(dayData);
    }

    if (m_storage && endOfDayData) {
        m_storage->setEndOfDayData(endOfDayData, m_serial);
    }
}

void SmaInverter::decodeMonthData(const ByteBuffer& buffer) {
    const uint recordsize = 12;
    m_pendingMonthData.reserve(buffer.size()/recordsize);

    std::time_t endOfMonthData = 0;
    for (uint x = 0; x < (buffer.size() - recordsize); x += recordsize) {
        auto datetime = (time_t)get_long(buffer.data() + x);
        auto totalWh = (unsigned long long)get_longlong(buffer.data() + x + 4);
        if (totalWh == NaN_U64) {
            endOfMonthData = std::max(endOfMonthData, datetime);
            continue;   // Fix Issue 109: Bad request 400: Power value too high for system size
        }

        MonthData monthData;
        monthData.datetime = datetime;
        monthData.totalWh = totalWh;
        monthData.serial = m_serial;
        //LOG_S(1) << monthData;
        m_pendingMonthData.push_back(monthData);
    }

    if (m_storage && endOfMonthData) {
        m_storage->setEndOfMonthData(endOfMonthData, m_serial);
    }
}

} // namespace sma
