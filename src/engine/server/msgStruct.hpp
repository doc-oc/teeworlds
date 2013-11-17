#ifndef SERIALIZATION_MESSAGES_HPP
#define SERIALIZATION_MESSAGES_HPP
#include <boost/serialization/vector.hpp>
#include <string>
#include <vector>

class SaveDetails{
	public:
		int OnlineID;
		bool gShotgun;
		bool gGrenade;
		bool gRifle;
		bool dFreeze;
		double x;
		double y;

		SaveDetails (){}

		SaveDetails (int OnlineIDIn, bool gShotgunIn, bool gGrenadeIn, bool gRifleIn, bool dFreezeIn, double xIn, double yIn){
			OnlineID = OnlineIDIn;
			gShotgun = gShotgunIn;
			gGrenade = gGrenadeIn;
			gRifle = gRifleIn;
			dFreeze =dFreezeIn;
			x = xIn;
			y = yIn;
		}

		template <typename Archive>
		void serialize (Archive &ar, const unsigned int version){
			ar & OnlineID;
			ar & gShotgun;
			ar & gGrenade;
			ar & gRifle;
			ar & dFreeze;
			ar & x;
			ar & y;
		}
};

enum {
	//Server -> External
	NET_SERVERSTART,
	NET_CLIENTDETAILSREQ,
	NET_SERVERLOG,
	NET_PLAYERLOG,
	NET_NAMEINFO,
	NET_BUGREPORT,
	NET_ADMINNOTIFICATION,
	NET_PLAYERRATING,
	NET_IMPLEMENTBAN,
	NET_MAPDETAILSREQ,
	NET_CHECKADDRESSBAN,
	NET_SENDTIME, 
	//DDrace
	NET_COMMENDATIONREQ,
	NET_MAPRECORD,
	NET_FETCHRANK,
	NET_SAVEDETAILS,
	NET_FETCHRUN,
	//zCatch
	NET_UPDATESTATS,
	NET_PROCESSSTATS,
	//openFNG
	NET_OPENUPDATESTATS,
	NET_OPENPROCSTATS,

	//External -> Server
	NET_SERVERDETAILS,
	NET_CLIENTDETAILS,
	NET_MAPDETAILS,
	NET_RATINGRESPONSE,
	NET_ADDRESSVALIDILITY,
	NET_BANLIST,
	//DDrace
	NET_COMMENDATIONRESPONSE,
	NET_PLAYERRANK,
	NET_LOADDETAILS,
	//zCatch - OpenFNG
	NET_PLAYERSTATS,
	
	NET_CLOSECON,
};

//Server -> External Server
struct ServerStart {
	std::string Address;
	int Port;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & Address;
		ar & Port;
	}
};

struct ClientDetailsRequest {
	std::string HashKey;
	int ClientID;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & HashKey;
		ar & ClientID;
	}
};

struct MapDetailsRequest{
	std::string MapName;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & MapName;
	}
};

struct ServerLog {
	std::string Category;
	std::string Text;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & Category;
		ar & Text;
	}
};

struct PlayerLog {
	int OnlineID;
	std::string Category;
	std::string Name;
	std::string Text;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & OnlineID;
		ar & Category;
		ar & Name;
		ar & Text;
	}
};

struct NameInfo {
	int OnlineID;
	std::string Address;
	std::string Name;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & OnlineID;
		ar & Address;
		ar & Name;
	}
};

struct BugReport {
	int OnlineID;
	std::string Report;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & OnlineID;
		ar & Report;
	}
};

struct AdminNotification{
	int OnlineID;
	std::string GameName;
	std::string Reason;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & OnlineID;
		ar & GameName;
		ar & Reason;
	}
};

struct PlayerRating{
	int OnlineID;
	int ClientID;
	int Score;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & OnlineID;
		ar & ClientID;
		ar & Score;
	}
};

struct ImplementBan {
	int OnlineID;
	std::string Address;
	std::string Name;
	int Minutes;
	std::string Reason;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & OnlineID;
		ar & Address;
		ar & Name;
		ar & Minutes;
		ar & Reason;
	}
};

struct CheckAddressBan{
	std::string Address;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & Address;
	}
};

struct SendTime{
	int OnlineID;
	std::string Date;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & OnlineID;
		ar & Date;
	}
};
//DDrace
struct CommendationReq{
	int OnlineID;
	int CommendID;
	int ClientID;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & OnlineID;
		ar & CommendID;
		ar & ClientID;
	}
};

struct MapRecord{
	int OnlineID;
	float Time;
	bool MapChallenge;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & OnlineID;
		ar & Time;
		ar & MapChallenge;
	}
};

struct FetchRank{
	int OnlineID;
	int ClientID;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & OnlineID;
		ar & ClientID;
	}
};

struct SaveRun{
	std::vector <SaveDetails> SavedPlayers;
	double Time;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & SavedPlayers;
		ar & Time;
	}
};

struct FetchRun{
	std::vector <int> OnlineIDs;
	int ClientID;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & OnlineIDs;
		ar & ClientID;
	}
};


//zCatch
struct UpdateStats{
	int OnlineID;
	int Kills;
	int Deaths;
	int Score;
	int Games;
	int Shots;
	int Wins;

	template<typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & OnlineID;
		ar & Kills;
		ar & Deaths;
		ar & Score;
		ar & Games;
		ar & Shots;
		ar & Wins;
	}
};

struct ProcessStats{
	int OnlineID;
	int ClientID;
	int Kills;
	int Deaths;
	int Score;
	int Games;
	int Shots;
	int Wins;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & OnlineID;
		ar & ClientID;
		ar & Kills;
		ar & Deaths;
		ar & Score;
		ar & Games;
		ar & Shots;
		ar & Wins;
	}
};

//openFNG
struct OpenUpdateStats{
	int OnlineID;
	int Kills;
	int Deaths;
	int Score;
	int Games;
	int Shots;
	int Wins;
	int Hits;
	int Saves;


	template<typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & OnlineID;
		ar & Kills;
		ar & Deaths;
		ar & Score;
		ar & Games;
		ar & Shots;
		ar & Wins;
		ar & Hits;
		ar & Saves;
	}
};

struct OpenProcStats{
	int OnlineID;
	int ClientID;
	int Kills;
	int Deaths;
	int Score;
	int Games;
	int Shots;
	int Wins;
	int Hits;
	int Saves;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & OnlineID;
		ar & ClientID;
		ar & Kills;
		ar & Deaths;
		ar & Score;
		ar & Games;
		ar & Shots;
		ar & Wins;
		ar & Hits;
		ar & Saves;
	}
};

//External Server -> Server
struct ServerDetails {
	std::string Name;
	int LogID;
	std::string Maps;
	std::string Commands;
	std::string BanList;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & Name;
		ar & LogID;
		ar & Maps;
		ar & Commands;
		ar & BanList;
	}
};

struct ClientDetails {
	int OnlineID;
	int ClientID;
	std::string Username;
	std::string Clan;
	int Auth;
	int Rank;
	int Registered;
	int Infractions;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & OnlineID;
		ar & ClientID;
		ar & Username;
		ar & Clan;
		ar & Auth;
		ar & Rank;
		ar & Registered;
		ar & Infractions;
	}
};

struct MapDetails{
	int MapMin;
	
	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & MapMin;
	}
};

struct RatingResponse {
	int ClientID;
	std::string Message;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & ClientID;
		ar & Message;
	}
};

struct AddressValidility{
	bool Valid;
	std::string Address;
	int Time;
	std::string Reason; 

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & Valid;
		ar & Address;
		ar & Time;
		ar & Reason;
	}
};

//DDrace
struct CommendationResponse{
	int ClientID;
	std::string Message;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & ClientID;
		ar & Message;
	}
};

struct PlayerRank{
	int ClientID;
	int Rank;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & ClientID;
		ar & Rank;
	}
};

struct LoadRun{
	int ClientID;
	std::vector <SaveDetails> SavedPlayers;
	double Time;

	template <typename Archive>
	void serialize (Archive &ar, const unsigned int version){
		ar & ClientID;
		ar & SavedPlayers;
		ar & Time;
	}
};


//zCatch - OpenFng
struct PlayerStats {
	int ClientID;
	std::string Stats;

	template <typename Archive>
	void serialize (Archive& ar, const unsigned int version){
		ar & ClientID;
		ar & Stats;
	}
};
#endif // SERIALIZATION_MESSAGES_HPP


