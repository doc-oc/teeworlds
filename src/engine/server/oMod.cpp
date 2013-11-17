#include "oMod.h"
#include <boost/archive/text_iarchive.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>

using boost::asio::ip::tcp;

#define SERVER "localhost"	
#define PORT "8001"
int m_LogID = 0;


void FetchDate (char * DateIn, int DateSize, bool shorten){
	time_t rawtime;
	struct tm* timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	if (shorten)
		strftime (DateIn,DateSize,"20%y_%m_%d",timeinfo);
	else
		strftime (DateIn,DateSize,"20%y-%m-%d %H:%M:%S",timeinfo);
}

void logFile (bool isFunction, char * logData){
	std::ofstream outfile;
	char aBuf [32];
	if (isFunction)
		str_format (aBuf, sizeof (aBuf), "%d_FLog.txt", m_LogID);
	else
		str_format (aBuf, sizeof (aBuf), "%d_SLog.txt", m_LogID);

	outfile.open(aBuf, std::ios_base::app);
	outfile << logData << "\n"; 
	outfile.close();
}

Session::Session(boost::asio::io_service& io_service,
     boost::asio::ip::tcp::resolver::iterator endpoint_iterator, DBConnector *connectorIn)
    : io_service_(io_service),
      socket_ (io_service)
	{
		connector = connectorIn;
    boost::asio::async_connect(socket_, endpoint_iterator,
        boost::bind(&Session::handle_connect, this,
          boost::asio::placeholders::error));
  }

void  Session::WriteMessage (std::vector<boost::asio::const_buffer> buffers){
	boost::asio::async_write(socket_,
          buffers,
          boost::bind(&Session::handle_write, this,
            boost::asio::placeholders::error));
  }

void  Session::handle_connect(const boost::system::error_code& error)
  {
    if (error)
    {
		delete this;
    }
	else{
		 socket_.async_read_some(boost::asio::buffer(inbound_header, header_length),
        boost::bind(&Session::handle_read, this, boost::asio::placeholders::error,  boost::asio::placeholders::bytes_transferred));
	}
  }

boost::asio::ip::tcp::socket&  Session::Socket()
  {
    return socket_;
  }

void  Session::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
  {
    if (!error)
    {
      if (error == boost::asio::error::eof)
        return; // Connection closed cleanly by peer.
      else if (error)
        throw boost::system::system_error(error); // Some other error.

	  // Determine the length of the serialized data.
      std::istringstream is(std::string(inbound_header, header_length));
      std::size_t inbound_data_size = 0;
      if (!(is >> std::hex >> inbound_data_size))
      {
        // Header doesn't seem to be valid. Inform the caller
        boost::system::error_code error(boost::asio::error::invalid_argument);
        throw boost::system::system_error(error); // Some other error.
      }

	  int MessageID = inbound_data_size%100;
	  inbound_data_size = (inbound_data_size-MessageID)/100;
      // Start an asynchronous call to receive the data.
      inbound_data_.resize(inbound_data_size);
	 socket_.async_read_some(boost::asio::buffer(inbound_data_, inbound_data_size),
        boost::bind(&Session::handle_read_body, this, MessageID, boost::asio::placeholders::error,  boost::asio::placeholders::bytes_transferred));
    }
    else
      throw boost::system::system_error(error); // Some other error.
  }
void  Session::handle_read_body (int MessageID, const boost::system::error_code& error, size_t bytes_transferred){
	if (!error){
		connector->Messages (inbound_data_, MessageID, error);
      // Inform caller that data has been received ok.
		socket_.async_read_some(boost::asio::buffer(inbound_header, header_length),
        boost::bind(&Session::handle_read, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}
	else
		throw boost::system::system_error(error); // Some other error.
}

  void  Session::handle_write(const boost::system::error_code& error)
  {
    if (error)
      throw boost::system::system_error(error); // Some other error.
  }

DBConnector::DBConnector (IServer *m_pServerIn, IConsole *m_pConsoleIn){

	m_pServer = m_pServerIn;
	m_pConsole = m_pConsoleIn;
	try{
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(SERVER, PORT);
		auto endpoint_iterator = resolver.resolve(query);
		session = new Session(io_service, endpoint_iterator, this);
		ThreadSleep ();
	}
	catch (std::exception& e){
		std::cerr << "Exception: " << e.what() << "\n";
	}
}


void DBConnector::OnTick (){
	io_service.poll_one ();
}

DBConnector::~DBConnector (){
	Close ();
}


template <typename T>
void DBConnector::DeserializeMsg (boost::system::error_code error, T& t){
	if (error)
		std::cout << "Error" << error.message () << std::endl;
	if (error == boost::asio::error::eof)
			return; // Connection closed cleanly by peer.
		  else if (error)
			throw boost::system::system_error(error); // Some other error.
		  // Extract the data structure from the data just received.
		  try
		  {
			std::string archive_data(&Data [0], Data.size());
			std::istringstream archive_stream(archive_data);
			boost::archive::text_iarchive archive(archive_stream);
			archive >> t;
		  }
		  catch (std::exception& e)
		  {
			  std::cout << "Error" << e.what () << std::endl;
			// Unable to decode data.
			boost::system::error_code error(boost::asio::error::invalid_argument);
			throw boost::system::system_error(error); // Some other error.
			return;
		  }
}

void DBConnector::Messages (std::vector<char> DataIn, int MessageID, boost::system::error_code error){
	try{
		Data = DataIn;
	switch (MessageID){
		case NET_SERVERDETAILS:{
			ServerDetails sDetailsStruct;
			DeserializeMsg (error, sDetailsStruct);
			sDetailsStruct.Commands += sDetailsStruct.Maps;
			sDetailsStruct.Commands += sDetailsStruct.BanList;

			m_LogID = sDetailsStruct.LogID;
			logFile (false, "Start UP");
			logFile (true, "Start UP");
			Server ()->ImportedCommands (sDetailsStruct.Name.c_str (), sDetailsStruct.Commands.c_str (), sDetailsStruct.LogID);
			break;
			}
		case NET_CLIENTDETAILS:{
			ClientDetails cDetailsStruct;
			DeserializeMsg (error, cDetailsStruct);
			Server ()->SetClientOnlineDetails (cDetailsStruct.OnlineID, cDetailsStruct.ClientID, cDetailsStruct.Auth, cDetailsStruct.Username.c_str (), cDetailsStruct.Clan.c_str (), cDetailsStruct.Rank, cDetailsStruct.Registered, cDetailsStruct.Infractions);
			break;
		}
		case NET_RATINGRESPONSE:{
			RatingResponse responseStruct;
			DeserializeMsg (error, responseStruct);
			char aBuf [64];
			str_format (aBuf, sizeof (aBuf), "say_target %d \"%s\"", responseStruct.ClientID, responseStruct.Message.c_str ());
			Console ()->ExecuteLine (aBuf);
			break;
		}
		case NET_MAPDETAILS:{
			MapDetails mDetailsStruct;
			DeserializeMsg (error, mDetailsStruct);
			char aBuf [32];
			str_format (aBuf, sizeof (aBuf), "map_min %d", mDetailsStruct.MapMin);
			Console ()->ExecuteLine (aBuf);
			break;
		}
		case NET_ADDRESSVALIDILITY:{
			AddressValidility addrValStruct;
			DeserializeMsg (error, addrValStruct);
			if (!addrValStruct.Valid){
				char aBuf [64];
				str_format (aBuf, sizeof (aBuf), "Ban %s %d \"%s\"", addrValStruct.Address.c_str (), addrValStruct.Time, addrValStruct.Reason.c_str ());
				Console ()-> ExecuteLine(aBuf);
			}
			break;
		}
		//DDrace
		case NET_COMMENDATIONRESPONSE:{
			CommendationResponse commendRespStruct;
			DeserializeMsg (error, commendRespStruct);
			char aBuf [64];
			str_format (aBuf, sizeof (aBuf), "say_target %d \"%s\"", commendRespStruct.ClientID, commendRespStruct.Message.c_str ());
			Console ()->ExecuteLine (aBuf);
			break;
		}
		case NET_PLAYERRANK:{
			PlayerRank pRank;
			DeserializeMsg (error, pRank);
			Server ()->PlayersRank (pRank.ClientID, pRank.Rank);
			break;
		}
		case NET_LOADDETAILS:{
			LoadRun runDetails;
			DeserializeMsg (error, runDetails);
			Server ()->LoadSave (runDetails);
			break;
		}
	}
	} catch (const std::exception &e) {
			char aBuf [64];
			str_format (aBuf, sizeof (aBuf), "oMsg - %d, Error - %s", MessageID, e.what()); 
			logFile (false, aBuf);
		} catch (const int i) {
			char aBuf [64];
			str_format (aBuf, sizeof (aBuf), "oMsg - %d, Error - %d", MessageID, i); 
			logFile (false, aBuf);
		} catch (const long l) {
			char aBuf [64];
			str_format (aBuf, sizeof (aBuf), "oMsg - %d, Error - %ld", MessageID, l); 
			logFile (false, aBuf);
		} catch (const char *p) {
			char aBuf [64];
			str_format (aBuf, sizeof (aBuf), "oMsg - %d, Error - %s", MessageID, p); 
			logFile (false, aBuf);
		} catch (...) {
			char aBuf [64];
			str_format (aBuf, sizeof (aBuf), "oMsg - %d, Error - Unknown", MessageID); 
			logFile (false, aBuf);
		}
}

void DBConnector::Close (){
	SendMessage (NET_CLOSECON, 0);
	session->Socket ().close ();
	io_service.stop ();
}

void DBConnector::ThreadSleep (){
	for (int i = 0 ; i < 500;  i++){
		OnTick ();
		boost::this_thread::sleep (boost::posix_time::milliseconds(10));
	}
}