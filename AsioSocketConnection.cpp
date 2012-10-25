#include "AsioSocketConnection.hpp"

namespace FIX
{
	AsioSocketConnection::AsioSocketConnection ( boost::shared_ptr < boost::asio::ip::tcp::socket > const & socket,
		SessionID const & sessionID,
		Session * session ) :
	m_socket ( socket ),
		m_session_id ( sessionID ),
		m_session ( session ),
		m_sending ( false )
	{
		assert ( m_socket.get() != 0 && m_socket->is_open() );
		assert ( session != 0 );
		session->setResponder ( this );
		session->next(); // should send logon
		Session::registerSession( m_session->getSessionID() );
		StartReadAsync();
	}

	AsioSocketConnection::~AsioSocketConnection()
	{
		Session::unregisterSession( m_session->getSessionID() );
		m_socket->cancel();
		disconnect();
	}

	bool AsioSocketConnection::send( const std::string& msg )
	{
		m_queued_outgoing_buffer.insert ( m_queued_outgoing_buffer.end(), msg.begin(), msg.end() );
		if ( !m_sending )
		{
			m_sending = true;
			StartSendAsync();
		}
		return true;
	}

	void AsioSocketConnection::disconnect()
	{
		m_socket->close();
	}

	void AsioSocketConnection::StartSendAsync()
	{
		assert ( m_sending );
		assert ( m_socket.get() != 0 && m_socket->is_open() );
		if ( m_queued_outgoing_buffer.size() == 0 ) 
			m_sending = false;
		else
		{
			size_t buffer_size ( m_queued_outgoing_buffer.size() > OUTGOING_BUFFER_SIZE ? OUTGOING_BUFFER_SIZE : m_queued_outgoing_buffer.size() );
			std::copy ( m_queued_outgoing_buffer.begin(), m_queued_outgoing_buffer.begin() + buffer_size, m_outgoing_buffer.begin() );
			m_socket->async_write_some ( boost::asio::buffer ( m_outgoing_buffer, buffer_size ), boost::bind ( &AsioSocketConnection::AsyncSentSocket,
				this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred ) );
		}
	}

	void AsioSocketConnection::StartReadAsync()
	{
		assert ( m_socket.get() != 0 && m_socket->is_open() );
		m_socket->async_read_some ( boost::asio::buffer ( m_incoming_buffer ),
			boost::bind ( &AsioSocketConnection::AsyncReadSocket,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred )
			);
	}

	void AsioSocketConnection::AsyncSentSocket( boost::system::error_code const & error_code, 
		size_t len )
	{
		if ( error_code == 0 && len != 0 )
		{
			m_queued_outgoing_buffer.erase( m_queued_outgoing_buffer.begin(), m_queued_outgoing_buffer.begin() + len );
			StartSendAsync();
		}
		else
		{
			std::cerr << "socket disconnected or error received!" << std::endl;
		}
	}

	void AsioSocketConnection::AsyncReadSocket( boost::system::error_code const & error_code, 
		size_t len )
	{
		if ( error_code == 0 && len != 0 )
		{
			assert ( m_socket.get() != 0 && m_socket->is_open() );
			const char* buffer ( m_incoming_buffer.begin() );
			m_parser.addToStream( buffer, len );
			assert ( m_session != 0 );
			std::string msg;
			while ( m_parser.readFixMessage( msg ) )
				m_session->next( msg, UtcTimeStamp() );
			StartReadAsync();
		}
		else
		{
			std::cerr << "socket disconnected or error received!" << std::endl;
		}
	}

}


