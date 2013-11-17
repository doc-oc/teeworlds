#ifndef oMOD_H
#define oMOD_H

#include "msgStruct.hpp"
#include <boost/asio.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <engine/server.h>
#include <engine/console.h>
#include <iostream>
#include <fstream>

struct CWordList{
	CWordList *m_pPrev;
	char m_aWord[16];
};

void FetchDate (char * DateIn, int DateSize, bool shorten);
void logFile (bool isFunction, char * logData);
enum { header_length = 8 };

class DBConnector;
class Session{
private:
  boost::asio::ip::tcp::socket socket_;
  boost::asio::io_service& io_service_;
  std::vector<char> inbound_data_;
  char inbound_header [header_length];
  DBConnector *connector;

public:
  Session(boost::asio::io_service& io_service, boost::asio::ip::tcp::resolver::iterator endpoint_iterator, DBConnector *connectorIn);
  void WriteMessage (std::vector<boost::asio::const_buffer> buffers);
  boost::asio::ip::tcp::socket&  Session::Socket();

private:
	void  Session::handle_connect(const boost::system::error_code& error);	
	void  Session::start();
	void  Session::handle_read(const boost::system::error_code& error, size_t bytes_transferred);
	void  Session::handle_read_body(int MessageID, const boost::system::error_code& error, size_t bytes_transferred);
	void  Session::handle_write(const boost::system::error_code& error);
};

class DBConnector {
	private:
		boost::asio::io_service io_service;
		IServer *m_pServer;
		IConsole *m_pConsole;
		Session *session;

		std::vector<char> Data;

		template <typename T>
		void DeserializeMsg (boost::system::error_code error, T& t);
		
		void ReadMessage ();

		IServer *Server() const { return m_pServer; }
		IConsole *Console() const { return m_pConsole; }

	public:
		DBConnector  (IServer *m_pServerIn, IConsole *m_pConsoleIn);
		~DBConnector ();

		void ThreadSleep ();
		void OnTick ();
		void Messages (std::vector<char> DataIn, int MessageID, boost::system::error_code error);
		template <typename T>
		void SendMessage (int MessageID, const T& t){
			// Serialize the data first so we know how large it is.
			std::string outbound_data_ = "";
			if (MessageID != NET_CLOSECON){
				std::ostringstream archive_stream;
				boost::archive::text_oarchive archive(archive_stream);
				archive << t;
				outbound_data_ = archive_stream.str();
			}
			// Format the header.
			std::ostringstream header_stream;
			header_stream << std::setw(header_length)
			  << std::hex << (outbound_data_.size()*100+MessageID);
			if (!header_stream || header_stream.str().size() != header_length)
			{
			  // Something went wrong, inform the caller.
			  boost::system::error_code error(boost::asio::error::invalid_argument);
			  return;
			}
			std::string outbound_header_ = header_stream.str();
			std::vector<boost::asio::const_buffer> buffers;
			buffers.push_back(boost::asio::buffer(outbound_header_));
			buffers.push_back(boost::asio::buffer(outbound_data_));
			session->WriteMessage (buffers);
		}

		void Close ();
};

#endif