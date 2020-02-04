#include <iostream>
#include <memory>

#include "Poco/JSON/JSON.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "Poco/Dynamic/Struct.h"
#include <Poco/Thread.h>
#include <Worker.h>

#include <Poco/Data/Session.h>
#include <Poco/Data/MySQL/Connector.h>
#include <Poco/Data/MySQL/MySQLException.h>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>

using namespace Poco::Data::Keywords;
using Poco::Data::MySQL::MySQLException;
using Poco::Data::MySQL::Connector;
using Poco::Data::Session;
using Poco::Thread;

#define MEM_FN(x)       boost::bind(&self_type::x, shared_from_this())
#define MEM_FN1(x,y)    boost::bind(&self_type::x, shared_from_this(),y)
#define MEM_FN2(x,y,z)  boost::bind(&self_type::x, shared_from_this(),y,z)

boost::asio::io_service service;										// Наш IoService
boost::asio::ip::tcp::endpoint ep(boost::asio::ip::tcp::v4(), 8811);	// Будем слушать порт 8811
boost::asio::ip::tcp::acceptor acceptor(service, ep);					// Серверный акцептор новых соединений

std::unique_ptr<Worker> w;

unsigned nextID = 1;

class talk_to_client : public boost::enable_shared_from_this<talk_to_client>, boost::noncopyable
{
    typedef talk_to_client self_type;
    talk_to_client() : sock_(service), started_(false), id_(nextID++) {}	// Закрытый конструктор
public:
    typedef boost::system::error_code error_code;
    typedef boost::shared_ptr<talk_to_client> ptr;

    void start() {
	started_ = true;
	std::cout << "New connection #" << id_ << " from: " << sock_.remote_endpoint().address().to_string() << std::endl;
	do_read();
    }
    static ptr new_() {
	ptr new_(new talk_to_client);
	return new_;
    }
    void stop() {
	if (!started_) return;
	started_ = false;
	sock_.close();
    }
    boost::asio::ip::tcp::socket & sock() { return sock_; }

private:
    void on_write(const error_code & err, size_t bytes) {
	do_read();
    }

    void do_read()
    {
	boost::asio::async_read(sock_, boost::asio::buffer(read_buffer_),
	    MEM_FN2(read_complete, _1, _2), MEM_FN2(on_read, _1, _2));
    }

    void on_read(const error_code & err, size_t bytes)
    {
	if (err == boost::asio::error::eof) {
	    std::cout << "Connection #" << id_ << " closed." << std::endl;
	    return; // Connection closed cleanly by peer.
	}
	    
	if (!err) {
	    std::string msg(read_buffer_, bytes);
	    Poco::JSON::Parser parser;
	    Poco::Dynamic::Var result;
	    try
            {
                    result = parser.parse(msg);
            }
            catch (Poco::JSON::JSONException& jsone)
            {
                std::cout << jsone.message() << std::endl;
		do_write(jsone.message());
		stop();
            }
            if(result.type() != typeid(Poco::JSON::Object::Ptr)) return;

	    Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();
    	    Poco::Dynamic::Var test = object->get("message");
	    if(test.isEmpty()) return;
	    std::cout << test.toString() << std::endl;
	    w->outStr(test.toString());
	    do_write("OK:" + test.toString() + "\n");
	}
	stop();
    }

    void do_write(const std::string & msg) {
	std::copy(msg.begin(), msg.end(), write_buffer_);
	sock_.async_write_some(boost::asio::buffer(write_buffer_, msg.size()),
	    MEM_FN2(on_write, _1, _2));
    }
    size_t read_complete(const boost::system::error_code & err, size_t bytes) {
	if (err) return 0;
	bool found = std::find(read_buffer_, read_buffer_ + bytes, '\n') < read_buffer_ + bytes;
	// we read one-by-one until we get to enter, no buffering
	return found ? 0 : 1;
    }
private:
    boost::asio::ip::tcp::socket sock_;
    enum { max_msg = 1024 };
    char read_buffer_[max_msg];
    char write_buffer_[max_msg];
    bool started_;
    unsigned id_;
};

// Обработчик нового соединения
void handle_accept(talk_to_client::ptr client, const boost::system::error_code & err)
{
    client->start();	// Запускаем обработку соединения

    //--- Готовимся к новому подключению
    talk_to_client::ptr new_client = talk_to_client::new_();
    acceptor.async_accept(new_client->sock(), boost::bind(handle_accept, new_client, _1));
}

int main(int argc, char* argv[])
{


    Poco::Data::MySQL::Connector::registerConnector();
    try {
        Session session(Connector::KEY, "host=localhost;port=3306;db=test;user=test;password=test;compress=true;auto-reconnect=true");
        w.reset(new Worker(session));
    }
    catch(MySQLException& e) {
        std::cerr << "Exception: " << e.displayText() << std::endl;
        return(-1);
    }


    w->start();

    talk_to_client::ptr client = talk_to_client::new_();
    acceptor.async_accept(client->sock(), boost::bind(handle_accept, client, _1));
    service.run();
    w->stop();

    return 0;
}