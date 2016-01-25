/*
* OpenClonk, http://www.openclonk.org
*
* Copyright (c) 2001-2009, RedWolf Design GmbH, http://www.clonk.de/
* Copyright (c) 2009-2013, The OpenClonk Team and contributors
*
* Distributed under the terms of the ISC license; see accompanying file
* "COPYING" for details.
*
* "Clonk" is a registered trademark of Matthes Bender, used with permission.
* See accompanying file "TRADEMARK" for details.
*
* To redistribute this file separately, substitute the full license texts
* for the above references.
*/

#include <C4Include.h>
#include "C4Network2Ping.h"


// C4Network2Ping

C4Network2Ping::C4Network2Ping()
	: index(0), sentPing(), receivedPong(), timeSent(), timeReceived(), addresses()
{
	Init();
	SetCallback(this);
}

bool C4Network2Ping::Ping(const addr_t &address)
{
	// Stub does not do any networking, yet

	// If there is a new discovery that just happened, then the addresses
	// will not be in the array.  If there is just a new ping measurement 
	// that needs to be done, the address will be in the array.
	int i = (GetIndex(address) < 0) ? index : GetIndex(address);
	if (i < MaxEntries)
	{
		addresses[i] = address;
		if (!Connect(address)) return false;
			
		const PingHdr pingPacket = { IPID_Ping | static_cast<uint8_t>(0x80u), 0 };
		const C4NetIOPacket packet(&pingPacket, sizeof(pingPacket), false, address);
		sentPing[i] = Send(packet);
		timeSent[i] = C4TimeMilliseconds::Now();
		if (i == index) index++;
		return true;
	}
	return false;
}

C4TimeMilliseconds C4Network2Ping::GetPing(addr_t address)
{
	return GetPing(GetIndex(address));
}

// The idea here is to do a reset of the ping when C4NetStartupDlg wants to
// run a discovery
void C4Network2Ping::Reset()
{
	// Close open connections
	for (int i = 0; i < index; i++)
	{
		if (sentPing[i] && !receivedPong[i]) Close(addresses[i]);
	}
	index = 0;
}

void C4Network2Ping::OnPacket(const C4NetIOPacket &rPacket, C4NetIO *pNetIO)
{
	if ((rPacket.getStatus() & 0x7F) == IPID_Ping)
	{
		addr_t addr = rPacket.getAddr();
		int i = GetIndex(rPacket.getAddr());
		if (i >= 0)
		{
			receivedPong[i] = true;
			timeReceived[i] = C4TimeMilliseconds::Now();
			Close(addr);
		}
	}
}

C4TimeMilliseconds C4Network2Ping::GetPing(int indexFound)
{
	if (indexFound < 0) return C4TimeMilliseconds(0);
	// Will eventually return the difference of the time sent and the time received
	if (sentPing[indexFound] && receivedPong[indexFound])
	{
		return timeReceived[indexFound] - timeSent[indexFound];
	}
	return C4TimeMilliseconds(0);
}

int C4Network2Ping::GetIndex(addr_t addr)
{
	for (int i = 0; i < index; i++)
	{
		if (addr == addresses[i]) return i;
	}
	return -1;
}

/*


void C4Network2PingClient::OnPacket(const C4NetIOPacket &rPacket, C4NetIO *pNetIO)
{
	// pong?
	if ((rPacket.getStatus() & 0x7F) == IPID_Ping)
	{
		int i = GetIndex(rPacket.getAddr());
		if (i >= 0)
		{
			receivedPong[i] = true;
			timeReceived[i] = C4TimeMilliseconds::Now;
		}
	}
}

bool C4Network2PingClient::Ping(addr_t destination)
{
	if (index >= MaxEntries)
		return false;
	addresses[index] = destination;
	// Following the setup for a ping packet from C4NetIOUDP
	if (!Connect(destination))
		return false;
	const PacketHdr PingPacket = { IPID_Ping | static_cast<uint8_t>(0x80u), 0 };
	const C4NetIOPacket packet(&PingPacket, sizeof(PingPacket), false, destination);
	sentPing[index] = Send(packet);
	timeSent[index] = C4TimeMilliseconds::Now;
	index++;
	return true;
}

C4TimeMilliseconds C4Network2PingClient::GetPing(int indexFound)
{
	if (!sentPing[indexFound] || !receivedPong[indexFound])
		return C4TimeMilliseconds(0);
	return timeReceived[indexFound] - timeSent[indexFound];
}
*/