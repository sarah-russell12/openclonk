/*
* OpenClonk, http://www.openclonk.org
*
* Copyright (c) 2001-2009, RedWolf Design GmbH, http://www.clonk.de/
* Copyright (c) 2013, The OpenClonk Team and contributors
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

#ifndef INC_C4Network2Ping
#define INC_C4Network2Ping

#include "C4TimeMilliseconds.h"

#include "C4NetIO.h"

// Low-level networking class that sends receives pong packets and
// measures round trip time

// This is intended to work with C4NetStartupDlg for the proposed ping feature

const int PingTimeout = 30000; // ms
const int MaxEntries = 64; // Maximum number of discoverie that can be made

class C4Network2Ping : public C4NetIOUDP, private C4NetIO::CBClass
{
public:
	C4Network2Ping();

	// Based on PacketHdr, which is only defined in C4NetIO.cpp
	struct PingHdr
	{
		uint8_t  StatusByte;
		uint32_t Nr;    // packet nr
	};

public:
	bool Ping(const addr_t &address);
	C4TimeMilliseconds GetPing(addr_t address);

	void Reset();

protected:
	virtual void OnPacket(const C4NetIOPacket &rPacket, C4NetIO *pNetIO);

private:
	C4TimeMilliseconds GetPing(int indexFound);
	int GetIndex(addr_t addr);

private:
	addr_t addresses[MaxEntries];

	bool sentPing[MaxEntries];
	bool receivedPong[MaxEntries];

	C4TimeMilliseconds timeSent[MaxEntries];
	C4TimeMilliseconds timeReceived[MaxEntries];

	int index;
};

/*
const int PingTimeout = 30000; // ms
const int MaxEntries = 64; // Maximum number of discoverie that can be made

class C4Network2PingClient : public C4NetIOUDP, private C4NetIO::CBClass
{
public:
	C4Network2PingClient();

	bool Ping(addr_t destination);

	C4TimeMilliseconds GetPing(addr_t address);

	// Resets the Ping Client with empty data
	void Clear();

protected:

	// Callbacks handle everything
	virtual void OnPacket(const C4NetIOPacket &rPacket, C4NetIO *pNetIO);

private:
	C4TimeMilliseconds GetPing(int indexFound);

	int GetIndex(addr_t address);

private:
	bool   sentPing[MaxEntries];
	bool   receivedPong[MaxEntries];
	addr_t addresses[MaxEntries];

	C4TimeMilliseconds timeSent[MaxEntries];
	C4TimeMilliseconds timeReceived[MaxEntries];
	
	int index;
};
*/
#endif