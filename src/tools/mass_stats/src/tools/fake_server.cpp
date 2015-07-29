/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <stdlib.h> //rand
#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/shared/network.h>
#include <mastersrv/mastersrv.h>
#include <stdio.h>

CNetServer *pNet;

int Progression = 50;
int GameType = 0;
int Flags = 0;

const char *pVersion = "trunk";
const char *pMap = "somemap";
const char *pServerName = "unnamed server";

NETADDR aMasterServers[16] = {{0,{0},0}};
int NumMasters = 0;

const char *PlayerNames[16] = {0};
int PlayerScores[16] = {0};
int NumPlayers = 0;
int MaxPlayers = 16;

char aInfoMsg[1024];
int aInfoMsgSize;

static void SendHeartBeats()
{
	static unsigned char aData[sizeof(SERVERBROWSE_HEARTBEAT) + 2];
	unsigned short Port = g_Config.m_SvPort;
	CNetChunk Packet;

	mem_copy(aData, SERVERBROWSE_HEARTBEAT, sizeof(SERVERBROWSE_HEARTBEAT));

	NETADDR Addr;
	net_host_lookup("master2.teeworlds.com", &Addr, NETTYPE_ALL);

	Packet.m_ClientID = -1;
	Packet.m_Address = Addr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;
	Packet.m_DataSize = sizeof(SERVERBROWSE_HEARTBEAT) + 2;
	Packet.m_pData = &aData;

	// supply the set port that the master can use if it has problems
	aData[sizeof(SERVERBROWSE_HEARTBEAT)] = Port >> 8;
	aData[sizeof(SERVERBROWSE_HEARTBEAT)+1] = Port&0xff;
	pNet->Send(&Packet);
}

static void WriteStr(const char *pStr)
{
	int l = str_length(pStr)+1;
	mem_copy(&aInfoMsg[aInfoMsgSize], pStr, l);
	aInfoMsgSize += l;
}

static void WriteInt(int i)
{
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%d", i);
	WriteStr(aBuf);
}

static void BuildInfoMsg()
{
	aInfoMsgSize = sizeof(SERVERBROWSE_INFO);
	mem_copy(aInfoMsg, SERVERBROWSE_INFO, aInfoMsgSize);
	WriteInt(-1);

	WriteStr(pVersion);
	WriteStr(pServerName);
	WriteStr(pMap);
	WriteInt(GameType);
	WriteInt(Flags);
	WriteInt(Progression);
	WriteInt(NumPlayers);
	WriteInt(MaxPlayers);

	for(int i = 0; i < NumPlayers; i++)
	{
		WriteStr(PlayerNames[i]);
		WriteInt(PlayerScores[i]);
	}
}

static void SendServerInfo(NETADDR *pAddr)
{
	CNetChunk p;
	p.m_ClientID = -1;
	p.m_Address = *pAddr;
	p.m_Flags = NETSENDFLAG_CONNLESS;
	p.m_DataSize = aInfoMsgSize;
	p.m_pData = aInfoMsg;
	pNet->Send(&p);
}

static void SendFWCheckResponse(NETADDR *pAddr)
{
	CNetChunk p;
	p.m_ClientID = -1;
	p.m_Address = *pAddr;
	p.m_Flags = NETSENDFLAG_CONNLESS;
	p.m_DataSize = sizeof(SERVERBROWSE_FWRESPONSE);
	p.m_pData = SERVERBROWSE_FWRESPONSE;
	pNet->Send(&p);
}

static int Run()
{
	int64 NextHeartBeat = 0;
	NETADDR BindAddr = {NETTYPE_IPV4, {0},0};

	if(!pNet->Open(BindAddr, 0, 0, 0, 0))
		return 0;

	while(1)
	{
		CNetChunk p;
		pNet->Update();
		while(pNet->Recv(&p))
		{
			if(p.m_ClientID == -1)
			{
				if(p.m_DataSize == sizeof(SERVERBROWSE_GETINFO) &&
					mem_comp(p.m_pData, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO)) == 0)
				{
					SendServerInfo(&p.m_Address);
					char aAddrStr[NETADDR_MAXSTRSIZE];
			        net_addr_str(&p.m_Address, aAddrStr, sizeof(aAddrStr), true);
			        printf("%s", aAddrStr);
				}
				else if(p.m_DataSize == sizeof(SERVERBROWSE_FWCHECK) &&
					mem_comp(p.m_pData, SERVERBROWSE_FWCHECK, sizeof(SERVERBROWSE_FWCHECK)) == 0)
				{
					SendFWCheckResponse(&p.m_Address);
				}
			}
		}

		/* send heartbeats if needed */
		if(NextHeartBeat < time_get())
		{
			NextHeartBeat = time_get()+time_freq()*20;
			SendHeartBeats();
			printf("Sent heartbeats");
			printf("\n");
		}

		thread_sleep(100);
	}
}

int main(int argc, char **argv)
{
	pNet = new CNetServer;

	BuildInfoMsg();
	int RunReturn = Run();

	delete pNet;
	return RunReturn;
}
