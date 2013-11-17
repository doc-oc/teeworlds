/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "teams.h"
#include <engine/shared/config.h>
#include <engine/server/oMod.h>

CGameTeams::CGameTeams(CGameContext *pGameContext) :
		m_pGameContext(pGameContext)
{
	Reset();
}

void CGameTeams::Reset()
{
	m_Core.Reset();
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		m_TeamState[i] = TEAMSTATE_EMPTY;
		m_TeamLocked [i] = 0;
		m_TeeFinished[i] = false;
		m_MembersCount[i] = 0;
		m_LastChat[i] = 0;
	}
}

void CGameTeams::OnCharacterStart(int ClientID)
{
	try{
	int Tick = Server()->Tick();
	CCharacter* pStartingChar = Character(ClientID);
	if (!pStartingChar)
		return;


	//oMod
	if (GetPlayer (ClientID)->m_MCState != MCSTATE_NONE){
		if (GetPlayer (ClientID)->m_MCState == MCSTATE_OPTED && (m_Core.Team(ClientID) == TEAM_FLOCK || GetPlayer (ClientID)->m_LockTeam)){
			GetPlayer (ClientID)->m_MCState = MCSTATE_WAITING;
			if (!GetPlayer (ClientID)->m_LockTeam){
				if (!GameServer()->m_MCStart){
					GameServer()->m_MCStart = Server()->TickSpeed() * 30 + Server()->Tick();
					GameServer()->SendChat(-1, CGameContext::CHAT_ALL, "Map Challenge pooling started, please wait 30 seconds for other players.");
				}
				else{
					if (GameServer () ->m_MCStart < 0)
						SetMCTeams (false);
				}		
			}
			else if (GetTeamLocked (GetPlayer (ClientID)->m_LockTeam))
				SetLockedTeam (ClientID);
		}
	}
	if (GetTeamLocked (GetPlayer (ClientID)->m_LockTeam))
		return;

	if (pStartingChar->m_DDRaceState == DDRACE_FINISHED)
		pStartingChar->m_DDRaceState = DDRACE_NONE;
	if (m_Core.Team(ClientID) == TEAM_FLOCK
			|| m_Core.Team(ClientID) == TEAM_SUPER)
	{
		pStartingChar->m_DDRaceState = DDRACE_STARTED;
		pStartingChar->m_StartTime = Tick;
	}
	else
	{
		bool Waiting = false;
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (m_Core.Team(ClientID) == m_Core.Team(i))
			{
				CPlayer* pPlayer = GetPlayer(i);
				if (pPlayer && pPlayer->IsPlaying()
						&& GetDDRaceState(pPlayer) == DDRACE_FINISHED)
				{
					Waiting = true;
					pStartingChar->m_DDRaceState = DDRACE_NONE;
					if (m_LastChat[ClientID] + Server()->TickSpeed()
							+ g_Config.m_SvChatDelay < Tick)
					{
						char aBuf[128];
						str_format(
								aBuf,
								sizeof(aBuf),
								"%s has finished and didn't go through start yet, wait for him or join another team.",
								Server()->ClientName(i));
						GameServer()->SendChatTarget(ClientID, aBuf);
						m_LastChat[ClientID] = Tick;
					}
					if (m_LastChat[i] + Server()->TickSpeed()
							+ g_Config.m_SvChatDelay < Tick)
					{
						char aBuf[128];
						str_format(
								aBuf,
								sizeof(aBuf),
								"%s wants to start a new round, kill or walk to start.",
								Server()->ClientName(ClientID));
						GameServer()->SendChatTarget(i, aBuf);
						m_LastChat[i] = Tick;
					}
				}
			}
		}

		if (m_TeamState[m_Core.Team(ClientID)] < TEAMSTATE_STARTED && !Waiting)
		{
			ChangeTeamState(m_Core.Team(ClientID), TEAMSTATE_STARTED);
			for (int i = 0; i < MAX_CLIENTS; ++i)
			{
				if (m_Core.Team(ClientID) == m_Core.Team(i))
				{
					CPlayer* pPlayer = GetPlayer(i);
					if (pPlayer && pPlayer->IsPlaying())
					{
						SetDDRaceState(pPlayer, DDRACE_STARTED);
						SetStartTime(pPlayer, Tick);
					}
				}
			}
		}
	}
		} catch (const std::exception &e) {
			char aBuf [64];
			str_format (aBuf, sizeof (aBuf), "Msg - %d, Error - %s", ClientID, e.what()); 
			logFile (true, aBuf);
		} catch (const int i) {
			char aBuf [64];
			str_format (aBuf, sizeof (aBuf), "Msg - %d, Error - %d", ClientID, i); 
			logFile (true, aBuf);
		} catch (const long l) {
			char aBuf [64];
			str_format (aBuf, sizeof (aBuf), "Msg - %d, Error - %ld", ClientID, l); 
			logFile (true, aBuf);
		} catch (const char *p) {
			char aBuf [64];
			str_format (aBuf, sizeof (aBuf), "Msg - %d, Error - %s", ClientID, p); 
			logFile (true, aBuf);
		} catch (...) {
			char aBuf [64];
			str_format (aBuf, sizeof (aBuf), "Msg - %d, Error - Unknown", ClientID); 
			logFile (true, aBuf);
		}
}

void CGameTeams::OnCharacterFinish(int ClientID)
{
	if (m_Core.Team(ClientID) == TEAM_FLOCK
			|| m_Core.Team(ClientID) == TEAM_SUPER)
	{
		CPlayer* pPlayer = GetPlayer(ClientID);
		if (pPlayer && pPlayer->IsPlaying())
			OnFinish(pPlayer);
	}
	else
	{
		m_TeeFinished[ClientID] = true;
		if (TeamFinished(m_Core.Team(ClientID)))
		{
			ChangeTeamState(m_Core.Team(ClientID), TEAMSTATE_FINISHED); //TODO: Make it better
			//ChangeTeamState(m_Core.Team(ClientID), TEAMSTATE_OPEN);
			for (int i = 0; i < MAX_CLIENTS; ++i)
			{
				if (m_Core.Team(ClientID) == m_Core.Team(i))
				{
					CPlayer* pPlayer = GetPlayer(i);
					if (pPlayer && pPlayer->IsPlaying())
					{
						OnFinish(pPlayer);
						m_TeeFinished[i] = false;
					}
				}
			}

		}
	}
}
//oMod
void CGameTeams::SetMCTeams (bool isFirst){
	int mapMin = Server ()->GetMapMin ();
	if (isFirst){
		int rankPlacing [MAX_CLIENTS];
		int playNum = 0;
		int waitingNum = 0;
		while (playNum != MAX_CLIENTS){
			if (GetPlayer (playNum)&&GetPlayer (playNum)->m_MCState == MCSTATE_WAITING && GetPlayer (playNum)->IsPlaying()&& !(GetPlayer (playNum)->m_LockTeam)){
				waitingNum++;

				int rank = Server()->GetClientsRank (playNum);
				for (int i = 0; i<waitingNum - 1; i++){
					if (rank < Server()->GetClientsRank (rankPlacing [i])){
						int j = waitingNum - 1;
						while (i != j){
							rankPlacing [j] = rankPlacing [j-1];
							j--;
						}
						rankPlacing [i] = playNum;
						rank = -1;
						break;
					}
				}
				if (rank != -1)
					rankPlacing [waitingNum - 1] = playNum;
				
			}
			playNum++;
		}
		if (waitingNum >= mapMin){
			for (int j = 0; (j+mapMin - 1)< waitingNum; j+= mapMin){
				int team = 1;
				while (team != MAX_CLIENTS && m_TeamState[team]!= TEAMSTATE_EMPTY)
					team++;
				int lowestTime = 0;
				for (int i = j; i < j+mapMin; i++){
					int compareTime = (Character(rankPlacing [i])->m_MCTimeRemove) ? (Server ()->Tick () - (Character(rankPlacing [i])->m_MCTimeRemove-Character (rankPlacing [i])->m_StartTime)) : Character (rankPlacing [i])->m_StartTime;
					if (Character (rankPlacing [i])->m_StartTime == 0)
						compareTime = Server ()->Tick ();
					if (lowestTime < compareTime)
						lowestTime = compareTime;
				}

				for (int i = j; i < j+mapMin; i++)
					StartPlayer (rankPlacing [i], lowestTime, team);
				ChangeTeamState(team, TEAMSTATE_STARTED);
			}
		}
	}
	else{
		int pos = 0;
		int waitingPlayers =0 ;
		while (pos != MAX_CLIENTS){
			if (GetPlayer (pos) && GetPlayer (pos)->IsPlaying() && GetPlayer (pos)->m_MCState == MCSTATE_WAITING && !(GetPlayer (pos)->m_LockTeam))
				waitingPlayers++;	
			pos++;
		}
		if (waitingPlayers >= mapMin){
			int team = 1;
			while (team != MAX_CLIENTS && m_TeamState[team]!= TEAMSTATE_EMPTY)
				team++;	
			for (int i = 0; i < MAX_CLIENTS; i++){
				if (GetPlayer(i)&&GetPlayer(i)->m_MCState == MCSTATE_WAITING && !GetPlayer (i)->m_LockTeam){
					StartPlayer (i, Server ()->Tick (), team);
					mapMin--;
				}

				if (mapMin == 0)
					break;
			}
			ChangeTeamState(team, TEAMSTATE_STARTED);
			SetMCTeams (false);
		}
	}
}
void CGameTeams::StartLockedPlayers (int ClientID, int Time){
	int team = 1;
	while (team != MAX_CLIENTS && m_TeamState[team]!= TEAMSTATE_EMPTY)
		team++;	
		
	int prevLockTeam = GetPlayer (ClientID)->m_LockTeam;
	ChangeTeamLocked (prevLockTeam, false);
	if (GetTeamState (prevLockTeam) == TEAMSTATE_LOCKED)
		ChangeTeamState (prevLockTeam, TEAMSTATE_EMPTY);

	for (int i = 0; i < MAX_CLIENTS; i++){
		if (GetPlayer(i)&&GetPlayer(i)->m_MCState == MCSTATE_WAITING && GetPlayer (i)->m_LockTeam == prevLockTeam){
			StartPlayer (i, Time, team);
			GetPlayer (i)->m_LockTeam = team;
		}
	}

	ChangeTeamLocked (team, true);
	ChangeTeamState(team, TEAMSTATE_STARTED);	
}

void CGameTeams::SetLockedTeam (int ClientID){
	int pos = 0;
	int lowestTime = 0;
	int notStarted =0 ;
	while (pos != MAX_CLIENTS){
		if (GetPlayer (pos) && GetPlayer (pos)->m_LockTeam == GetPlayer (ClientID)->m_LockTeam && !(GetPlayer (pos)->m_MCState == MCSTATE_WAITING))
				notStarted++;
		pos++;
	}
	
	if (notStarted){
		if (notStarted == 1)
			GameServer()->SendChatTarget(ClientID, "Waiting for last locked teamate.");
		else{
			char aBuf [36];
			str_format (aBuf, sizeof (aBuf), "Waiting on %d teamates", notStarted);
			GameServer()->SendChatTarget(ClientID, aBuf);
		}
	}
	else{		
		StartLockedPlayers (ClientID, Server ()->Tick ());
	}
}

void CGameTeams::StartPlayer (int ClientID, int Time, int Team){//oMod
	SetForceCharacterTeam (ClientID, Team);
	Character (ClientID)->m_StartTime = Time;
	Character (ClientID)->m_DDRaceState = DDRACE_STARTED;
	GetPlayer(ClientID)->m_MCState = MCSTATE_STARTED;
	GetPlayer(ClientID)->ProcessPause ();
	GetPlayer(ClientID)->m_Paused = 0;	
}

bool CGameTeams::SetCharacterTeam(int ClientID, int Team)
{
	//Check on wrong parameters. +1 for TEAM_SUPER
	if (ClientID < 0 || ClientID >= MAX_CLIENTS || Team < 0
			|| Team >= MAX_CLIENTS + 1)
		return false;
	//You can join to TEAM_SUPER at any time, but any other group you cannot if it started
	if (Team != TEAM_SUPER && m_TeamState[Team] > TEAMSTATE_OPEN)
		return false;
	//No need to switch team if you there
	if (m_Core.Team(ClientID) == Team)
		return false;
	//You cannot be in TEAM_SUPER if you not super
	if (Team == TEAM_SUPER && !Character(ClientID)->m_Super)
		return false;
	//if you begin race
	if (Character(ClientID)->m_DDRaceState != DDRACE_NONE)
	{
		//you will be killed if you try to join FLOCK
		if (Team == TEAM_FLOCK && m_Core.Team(ClientID) != TEAM_FLOCK)
			GetPlayer(ClientID)->KillCharacter(WEAPON_GAME);
		else if (Team != TEAM_SUPER)
			return false;
	}
	SetForceCharacterTeam(ClientID, Team);

	//GameServer()->CreatePlayerSpawn(Character(id)->m_Core.m_Pos, TeamMask());
	return true;
}

void CGameTeams::SetForceCharacterTeam(int ClientID, int Team)
{
	m_TeeFinished[ClientID] = false;
	if (m_Core.Team(ClientID) != TEAM_FLOCK
			&& m_Core.Team(ClientID) != TEAM_SUPER
			&& m_TeamState[m_Core.Team(ClientID)] != TEAMSTATE_EMPTY)
	{
		bool NoOneInOldTeam = true;
		for (int i = 0; i < MAX_CLIENTS; ++i)
			if (i != ClientID && m_Core.Team(ClientID) == m_Core.Team(i))
			{
				NoOneInOldTeam = false; //all good exists someone in old team
				break;
			}
		if (NoOneInOldTeam){
			if (m_TeamLocked[m_Core.Team(ClientID)])
				m_TeamState[m_Core.Team(ClientID)] = TEAMSTATE_LOCKED;
			else
				m_TeamState[m_Core.Team(ClientID)] = TEAMSTATE_EMPTY;
		}
	}
	if (Count(m_Core.Team(ClientID)) > 0)
		m_MembersCount[m_Core.Team(ClientID)]--;
	m_Core.Team(ClientID, Team);
	if (m_Core.Team(ClientID) != TEAM_SUPER)
		m_MembersCount[m_Core.Team(ClientID)]++;
	if (Team != TEAM_SUPER && m_TeamState[Team] == TEAMSTATE_EMPTY)
		ChangeTeamState(Team, TEAMSTATE_OPEN);
	for (int LoopClientID = 0; LoopClientID < MAX_CLIENTS; ++LoopClientID)
	{
		if (GetPlayer(LoopClientID)
				&& GetPlayer(LoopClientID)->m_IsUsingDDRaceClient)
			SendTeamsState(LoopClientID);
	}
}

int CGameTeams::Count(int Team) const
{
	if (Team == TEAM_SUPER)
		return -1;
	return m_MembersCount[Team];
}

void CGameTeams::ChangeTeamState(int Team, int State)
{
	int OldState = m_TeamState[Team];
	m_TeamState[Team] = State;
	onChangeTeamState(Team, State, OldState);
}

void CGameTeams::onChangeTeamState(int Team, int State, int OldState)
{
	if (OldState != State && State == TEAMSTATE_STARTED)
	{
		// OnTeamStateStarting
	}
	if (OldState != State && State == TEAMSTATE_FINISHED)
	{
		// OnTeamStateFinishing
	}
}

bool CGameTeams::TeamFinished(int Team)
{
	for (int i = 0; i < MAX_CLIENTS; ++i)
		if (m_Core.Team(i) == Team && !m_TeeFinished[i])
			return false;
	return true;
}

int CGameTeams::TeamMask(int Team, int ExceptID, int Asker)
{
	if (Team == TEAM_SUPER)
		return -1;
	if (m_Core.GetSolo(Asker) && ExceptID == Asker)
		return 0;
	if (m_Core.GetSolo(Asker))
		return 1 << Asker;
	int Mask = 0;
	for (int i = 0; i < MAX_CLIENTS; ++i)
		if (i != ExceptID)
			if ((Asker == i || !m_Core.GetSolo(i))
					&& ((Character(i)
							&& (m_Core.Team(i) == Team
									|| m_Core.Team(i) == TEAM_SUPER))
							|| (GetPlayer(i) && GetPlayer(i)->GetTeam() == -1)))
				Mask |= 1 << i;
	return Mask;
}

void CGameTeams::SendTeamsState(int ClientID)
{
	CNetMsg_Cl_TeamsState Msg;
	Msg.m_Tee0 = m_Core.Team(0);
	Msg.m_Tee1 = m_Core.Team(1);
	Msg.m_Tee2 = m_Core.Team(2);
	Msg.m_Tee3 = m_Core.Team(3);
	Msg.m_Tee4 = m_Core.Team(4);
	Msg.m_Tee5 = m_Core.Team(5);
	Msg.m_Tee6 = m_Core.Team(6);
	Msg.m_Tee7 = m_Core.Team(7);
	Msg.m_Tee8 = m_Core.Team(8);
	Msg.m_Tee9 = m_Core.Team(9);
	Msg.m_Tee10 = m_Core.Team(10);
	Msg.m_Tee11 = m_Core.Team(11);
	Msg.m_Tee12 = m_Core.Team(12);
	Msg.m_Tee13 = m_Core.Team(13);
	Msg.m_Tee14 = m_Core.Team(14);
	Msg.m_Tee15 = m_Core.Team(15);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

}

int CGameTeams::GetDDRaceState(CPlayer* Player)
{
	if (!Player)
		return DDRACE_NONE;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		return pChar->m_DDRaceState;
	return DDRACE_NONE;
}

void CGameTeams::SetDDRaceState(CPlayer* Player, int DDRaceState)
{
	if (!Player)
		return;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		pChar->m_DDRaceState = DDRaceState;
}

int CGameTeams::GetStartTime(CPlayer* Player)
{
	if (!Player)
		return 0;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		return pChar->m_StartTime;
	return 0;
}

void CGameTeams::SetStartTime(CPlayer* Player, int StartTime)
{
	if (!Player)
		return;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		pChar->m_StartTime = StartTime;
}

void CGameTeams::SetCpActive(CPlayer* Player, int CpActive)
{
	if (!Player)
		return;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		pChar->m_CpActive = CpActive;
}

float *CGameTeams::GetCpCurrent(CPlayer* Player)
{
	if (!Player)
		return NULL;

	CCharacter* pChar = Player->GetCharacter();
	if (pChar)
		return pChar->m_CpCurrent;
	return NULL;
}

void CGameTeams::OnFinish(CPlayer* Player)
{
	if (!Player || !Player->IsPlaying())
		return;
	//TODO:DDRace:btd: this ugly
	float time = (float) (Server()->Tick() - GetStartTime(Player))
			/ ((float) Server()->TickSpeed());
	if (time < 0.000001f)
		return;
	CPlayerData *pData = GameServer()->Score()->PlayerData(Player->GetCID());
	char aBuf[128];
	SetCpActive(Player, -2);
	str_format(aBuf, sizeof(aBuf),
			"%s finished in: %d minute(s) %5.2f second(s)",
			Server()->ClientName(Player->GetCID()), (int) time / 60,
			time - ((int) time / 60 * 60));
	if (Server ()->GetOnlineID (Player->GetCID())){
		if (Player->m_LockTeam && NumberOfLocked (m_Core.Team(Player->GetCID())) > Server ()->GetMapMin ())
			Server ()->AddRecord (Server ()->GetOnlineID (Player->GetCID()), time, 0);
		else
			Server ()->AddRecord (Server ()->GetOnlineID (Player->GetCID()), time, Player->m_MCState);
	}
	if (Player->m_MCState == MCSTATE_STARTED || Player->m_MCState == MCSTATE_WAITING)
		Player->m_MCState = MCSTATE_OPTED;
	if (g_Config.m_SvHideScore)
		GameServer()->SendChatTarget(Player->GetCID(), aBuf);
	else
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);

	if (time - pData->m_BestTime < 0)
	{
		// new record \o/
		str_format(aBuf, sizeof(aBuf), "New record: %5.2f second(s) better.",
				fabs(time - pData->m_BestTime));
		if (g_Config.m_SvHideScore)
			GameServer()->SendChatTarget(Player->GetCID(), aBuf);
		else
			GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
	}
	else if (pData->m_BestTime != 0) // tee has already finished?
	{
		if (fabs(time - pData->m_BestTime) <= 0.005)
		{
			GameServer()->SendChatTarget(Player->GetCID(),
					"You finished with your best time.");
		}
		else
		{
			str_format(aBuf, sizeof(aBuf),
					"%5.2f second(s) worse, better luck next time.",
					fabs(pData->m_BestTime - time));
			GameServer()->SendChatTarget(Player->GetCID(), aBuf); //this is private, sent only to the tee
		}
	}

	bool CallSaveScore = false;
#if defined(CONF_SQL)
	CallSaveScore = g_Config.m_SvUseSQL;
#endif

	if (!pData->m_BestTime || time < pData->m_BestTime)
	{
		// update the score
		pData->Set(time, GetCpCurrent(Player));
		CallSaveScore = true;
	}

	if (CallSaveScore)
		if (g_Config.m_SvNamelessScore || str_comp_num(Server()->ClientName(Player->GetCID()), "nameless tee",
				12) != 0)
			GameServer()->Score()->SaveScore(Player->GetCID(), time,
					GetCpCurrent(Player));

	bool NeedToSendNewRecord = false;
	// update server best time
	if (GameServer()->m_pController->m_CurrentRecord == 0
			|| time < GameServer()->m_pController->m_CurrentRecord)
	{
		// check for nameless
		if (g_Config.m_SvNamelessScore || str_comp_num(Server()->ClientName(Player->GetCID()), "nameless tee",
				12) != 0)
		{
			GameServer()->m_pController->m_CurrentRecord = time;
			//dbg_msg("character", "Finish");
			NeedToSendNewRecord = true;
		}
	}

	SetDDRaceState(Player, DDRACE_FINISHED);
	// set player score
	if (!pData->m_CurrentTime || pData->m_CurrentTime > time)
	{
		pData->m_CurrentTime = time;
		NeedToSendNewRecord = true;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (GetPlayer(i) && GetPlayer(i)->m_IsUsingDDRaceClient)
			{
				if (!g_Config.m_SvHideScore || i == Player->GetCID())
				{
					CNetMsg_Sv_PlayerTime Msg;
					Msg.m_Time = time * 100.0;
					Msg.m_ClientID = Player->GetCID();
					Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
				}
			}
		}
	}

	if (NeedToSendNewRecord && Player->m_IsUsingDDRaceClient)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (GameServer()->m_apPlayers[i]
					&& GameServer()->m_apPlayers[i]->m_IsUsingDDRaceClient)
			{
				GameServer()->SendRecord(i);
			}
		}
	}

	if (Player->m_IsUsingDDRaceClient)
	{
		CNetMsg_Sv_DDRaceTime Msg;
		Msg.m_Time = (int) (time * 100.0f);
		Msg.m_Check = 0;
		Msg.m_Finish = 1;

		if (pData->m_BestTime)
		{
			float Diff = (time - pData->m_BestTime) * 100;
			Msg.m_Check = (int) Diff;
		}

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, Player->GetCID());
	}

	int TTime = 0 - (int) time;
	if (Player->m_Score < TTime)
		Player->m_Score = TTime;

}

void CGameTeams::OnCharacterSpawn(int ClientID)
{
	m_Core.SetSolo(ClientID, false);
	SetForceCharacterTeam(ClientID, 0);
}

void CGameTeams::OnCharacterDeath(int ClientID)
{
	m_Core.SetSolo(ClientID, false);
	SetForceCharacterTeam(ClientID, 0);
}

//oMod
void CGameTeams::ChangeTeamLocked (int Team, bool LockedState){
	m_TeamLocked[Team] = LockedState;
}

int CGameTeams::NumberOfLocked (int LockedTeam){
	int numLocked = 0;
	for (int i = 0; i < MAX_CLIENTS; ++i){
		if (GetPlayer (i) && GetPlayer (i)->m_LockTeam == LockedTeam)
			numLocked++;
	}
	return numLocked;
}

bool CGameTeams::GetTeamLocked (int Team){
	return m_TeamLocked[Team];
}