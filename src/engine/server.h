/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_H
#define ENGINE_SERVER_H
#include "kernel.h"
#include "message.h"
#include <engine/server/msgStruct.hpp>

class IServer : public IInterface
{
	MACRO_INTERFACE("server", 0)
protected:
	int m_CurrentGameTick;
	int m_TickSpeed;

public:
	/*
		Structure: CClientInfo
	*/
	struct CClientInfo
	{
		const char *m_pName;
		int m_Latency;
	};

	int Tick() const { return m_CurrentGameTick; }
	int TickSpeed() const { return m_TickSpeed; }

	virtual int MaxClients() const = 0;
	virtual const char *ClientName(int ClientID) = 0;
	virtual const char *ClientClan(int ClientID) = 0;
	virtual int ClientCountry(int ClientID) = 0;
	virtual bool ClientIngame(int ClientID) = 0;
	virtual int GetClientInfo(int ClientID, CClientInfo *pInfo) = 0;
	virtual void GetClientAddr(int ClientID, char *pAddrStr, int Size) = 0;

	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID) = 0;

	template<class T>
	int SendPackMsg(T *pMsg, int Flags, int ClientID)
	{
		CMsgPacker Packer(pMsg->MsgID());
		if(pMsg->Pack(&Packer))
			return -1;
		return SendMsg(&Packer, Flags, ClientID);
	}

	virtual void SetClientName(int ClientID, char const *pName) = 0;
	virtual void SetClientClan(int ClientID, char const *pClan) = 0;
	virtual void SetClientCountry(int ClientID, int Country) = 0;
	virtual void SetClientScore(int ClientID, int Score) = 0;

	virtual int SnapNewID() = 0;
	virtual void SnapFreeID(int ID) = 0;
	virtual void *SnapNewItem(int Type, int ID, int Size) = 0;

	virtual void SnapSetStaticsize(int ItemType, int Size) = 0;

	enum
	{
		RCON_CID_SERV=-1,
		RCON_CID_VOTE=-2,
	};
	virtual void SetRconCID(int ClientID) = 0;
	virtual bool IsAuthed(int ClientID) = 0;
	virtual void Kick(int ClientID, const char *pReason) = 0;

	virtual void DemoRecorder_HandleAutoStart() = 0;
	virtual bool DemoRecorder_IsRecording() = 0;

	// DDRace

	virtual void GetClientAddr(int ClientID, NETADDR *pAddr) = 0;
	
	//oMod
	virtual void ImportedCommands (const char* ServerName, const char *Commands, int LogID) = 0;
	virtual void SetClientOnlineDetails (int OnlineID, int ClientID, int Auth, const char * Username, const char * Clan, int Rank, bool Registered, int Infractions) = 0;
	virtual void PlayersRank (int ClientID, int Rank) = 0;
	virtual	bool isGuest (int ClientID) = 0;
	virtual bool isTrial (int ClientID) = 0;
	virtual bool isVeteran(int ClientID) = 0; 

	virtual void ReplaceWords (char *pMsgIn, char replaceWith, bool incPunct) = 0; 
	virtual int GetOnlineID(int ClientID) = 0;
	virtual int GetMapMin () = 0;
	virtual int GetClientsRank (int ClientID) = 0;
	virtual int GetRegistered (int ClientID) = 0;
	virtual void SendPLog (int OnlineID, const char *Category, const char * Name, const char * Text) = 0;
	virtual void SendSLog (const char *Category, const char * Text) = 0;
	virtual void SendCommendation (int ClientID, int CommendID)= 0;
	virtual void SendAdminNotification (int OnlineID, const char * Username, const char * Reason)= 0;
	virtual void GetRank (int ClientID)= 0;
	virtual void SendBug (int OnlineID, const char * Report)= 0;
	virtual void SendRating (int ClientID, int Score)= 0;
	virtual void AddRecord (int OnlineID, float Time, bool MapChallenge)= 0;
	virtual void SendSave (SaveRun SaveDetails)=0;
	virtual void FetchSave (int ClientID, const int *OnlineIDs, int Size)=0;
	virtual void LoadSave (LoadRun SaveDetails)=0;
};

class IGameServer : public IInterface
{
	MACRO_INTERFACE("gameserver", 0)
protected:
public:
	virtual void OnInit() = 0;
	virtual void OnConsoleInit() = 0;
	virtual void OnShutdown() = 0;

	virtual void OnTick() = 0;
	virtual void OnPreSnap() = 0;
	virtual void OnSnap(int ClientID) = 0;
	virtual void OnPostSnap() = 0;

	virtual void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) = 0;

	virtual void OnClientConnected(int ClientID) = 0;
	virtual void OnClientEnter(int ClientID) = 0;
	virtual void OnClientDrop(int ClientID, const char *pReason) = 0;
	virtual void OnClientDirectInput(int ClientID, void *pInput) = 0;
	virtual void OnClientPredictedInput(int ClientID, void *pInput) = 0;

	virtual bool IsClientReady(int ClientID) = 0;
	virtual bool IsClientPlayer(int ClientID) = 0;

	virtual const char *GameType() = 0;
	virtual const char *Version() = 0;
	virtual const char *NetVersion() = 0;

	// DDRace

	virtual void OnSetAuthed(int ClientID, int Level) = 0;

	//oMod
	virtual void LoadPlayers (LoadRun Details) = 0;
};

extern IGameServer *CreateGameServer();
#endif
