/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA solar inverters
    (c)2012-2020, SBF

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

#include "Ethernet.h"

#include "osselect.h"

#ifdef WIN32

// Ignore warning C4127: conditional expression is constant
#pragma warning(disable: 4127)

#include <WinSock2.h>
#include <ws2tcpip.h>

//Windows Sockets Error Codes
//http://msdn.microsoft.com/en-us/library/ms740668(v=vs.85).aspx

#endif	/* WIN32 */

#if defined (linux) || defined (__APPLE__)
#include <sys/select.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#endif	// #if defined (linux) || defined (__APPLE__)

#include <stdio.h>
#include <ctype.h>
#include <iostream>

#include "Config.h"
#include "Defines.h"
#include "SBFNet.h"
#include "endianness.h"
#include "misc.h"

struct sockaddr_in addr_in, addr_out;

int Ethernet::ethConnect(short port)
{
    int ret = 0;

#ifdef WIN32
    WSADATA wsa;

    if (DEBUG_NORMAL) printf("Initialising Winsock...\n");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        printf("Failed while WSAStartup. Error Code : %d\n",WSAGetLastError());
        return -1;
    }
#endif
    // create socket for UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
    {
        printf ("Socket error : %s\n", strerror(errno));
        return -1;
    }

    // set up parameters for UDP
    memset((char *)&addr_out, 0, sizeof(addr_out));
    addr_out.sin_family = AF_INET;
    addr_out.sin_port = htons(port);
    addr_out.sin_addr.s_addr = htonl(INADDR_ANY);
    ret = bind(sock, (struct sockaddr*) &addr_out, sizeof(addr_out));
    // here is the destination IP
    addr_out.sin_addr.s_addr = inet_addr(IP_Broadcast);

    // set options to receive broadcasted packets
    // leave this block and you have normal UDP communication (after the inverter scan)
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = inet_addr(IP_Broadcast);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    ret = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq));
    unsigned char loop = 0;
    ret = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *)&loop, sizeof(loop));

    if (ret < 0)
    {
        printf ("setsockopt IP_ADD_MEMBERSHIP failed\n");
        return -1;
    }
    // end of setting broadcast options

    return 0; //OK
}

ByteBuffer Ethernet::ethRead()
{
    ByteBuffer buf(2048);

    int bytes_read;
    short timeout = 5;
    int8_t emCount = 5;
    socklen_t addr_in_len = sizeof(addr_in);

    fd_set readfds;

    do
    {
        struct timeval tv;
        tv.tv_sec = timeout;     //set timeout of reading
        tv.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        int rc = select(sock+1, &readfds, NULL, NULL, &tv);
        if (DEBUG_HIGHEST) printf("select() returned %d\n", rc);
        if (rc == -1)
        {
            printf ("select() error : %s\n", strerror(errno));
        }

        if (FD_ISSET(sock, &readfds))
            bytes_read = recvfrom(sock, buf.data(), buf.size(), 0, (struct sockaddr *)&addr_in, &addr_in_len);
        else
        {
            if (DEBUG_HIGHEST) puts("Timeout reading socket");
            return {};
        }

        if ( bytes_read > 0)
        {
            if (bytes_read > MAX_CommBuf)
            {
                MAX_CommBuf = bytes_read;
                if (DEBUG_NORMAL)
                    printf("MAX_CommBuf is now %d bytes\n", MAX_CommBuf);
            }
            if (DEBUG_NORMAL)
            {
                printf("Received %d bytes from IP [%s]\n", bytes_read, inet_ntoa(addr_in.sin_addr));
                if (bytes_read == 600 || bytes_read == 608 || bytes_read == 0)
                    printf(" ==> packet ignored\n");
            }
        }
        else
            printf("recvfrom() returned an error: %d\n", bytes_read);

    } while ((bytes_read == 600 || bytes_read == 608) && --emCount > 0); // keep on reading if data received from Energy Meter (600 bytes) or Sunny Home Manager (608 bytes)

    if (bytes_read == 600 || bytes_read == 608)
        bytes_read = 0;

    buf.resize(bytes_read);
    return buf;
}

int Ethernet::ethSend(const ByteBuffer& buffer, const std::string& toIP)
{
    if (DEBUG_NORMAL) HexDump(buffer, 10);

    addr_out.sin_addr.s_addr = inet_addr(toIP.c_str());
    size_t bytes_sent = sendto(sock, (const char*)buffer.data(), buffer.size(), 0, (struct sockaddr *)&addr_out, sizeof(addr_out));

    if (DEBUG_NORMAL) std::cout << bytes_sent << " Bytes sent to IP [" << inet_ntoa(addr_out.sin_addr) << "]" << std::endl;

    return bytes_sent;
}

#ifdef WIN32
int Ethernet::ethClose()
{
    if (sock != 0)
    {
        closesocket(sock);
        sock = 0;
    }

    return 0;
}

#endif

#if defined (linux) || defined (__APPLE__)
int Ethernet::ethClose()
{
    if (sock != 0)
    {
        close(sock);
        sock = 0;
    }
    return 0;
}
#endif

E_SBFSPOT Ethernet::ethGetPacket(Buffer& out)
{
    if (DEBUG_NORMAL) printf("ethGetPacket()\n");
    E_SBFSPOT rc = E_OK;

    do
    {
        auto buffer = ethRead();
        ethPacketHeaderL1L2 *pkHdr = (ethPacketHeaderL1L2 *)buffer.data();

        if (buffer.size() <= 0)
        {
            if (DEBUG_NORMAL) printf("No data!\n");
            rc = E_NODATA;
        }
        else
        {
            unsigned short pkLen = (pkHdr->pcktHdrL1.hiPacketLen << 8) + pkHdr->pcktHdrL1.loPacketLen;

            //More data after header?
            if (pkLen > 0)
            {
                if (DEBUG_HIGH) HexDump(buffer, 10);
                if (btohl(pkHdr->pcktHdrL2.MagicNumber) == ETH_L2SIGNATURE)
                {
                    out.clear();
                    // Copy CommBuf to packetbuffer
                    // Dummy byte to align with BTH (7E)
                    out.data().push_back(0);
                    // We need last 6 bytes of ethPacketHeader too
                    out.data().insert(out.data().end(), buffer.begin() + sizeof(ethPacketHeaderL1), buffer.end());
                    // Point packetposition at last byte in our buffer
                    // This is different from BTH
                    packetposition = buffer.size() - sizeof(ethPacketHeaderL1);

                    if (DEBUG_HIGH)
                    {
                        printf("<<<====== Content of pcktBuf =======>>>\n");
                        HexDump(out.data(), 10);
                        printf("<<<=================================>>>\n");
                    }

                    rc = E_OK;
                }
                else
                {
                    if (DEBUG_NORMAL) printf("L2 header not found.\n");
                    rc = E_RETRY;
                }
            }
            else
                rc = E_NODATA;
        }
    } while (rc == E_RETRY);

    return rc;
}
