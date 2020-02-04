//
// Created by achernov on 04.02.2020.
//

#ifndef DEMO_SERVER_WORKER_H
#define DEMO_SERVER_WORKER_H

#include <atomic>
#include <thread>
#include <mutex>

#include <Poco/Data/Session.h>

using namespace Poco::Data::Keywords;
using Poco::Data::Statement;

struct StringNode
{
    std::string 	msg;
    StringNode*		next;
};

class Worker
{
public:
    Worker(Poco::Data::Session session) : m_head(new StringNode), m_tail(m_head),
                        m_stop_flag(false), m_session(session)
    {
	m_head->next = nullptr;
    }

    ~Worker()
    {
        if (m_stop_flag != true) {
	    m_stop_flag = true;
	    stop();
	}

	if (m_head == m_tail) {
	    if (m_tail != nullptr) { delete m_tail; m_tail = nullptr; m_head = nullptr; }
	} else if (m_head != m_tail) {
	    if (m_head != nullptr) { delete m_head; m_head = nullptr; }
	    if (m_tail != nullptr) { delete m_tail; m_tail = nullptr; }
	}
    }

    void start() {
	m_stop_flag = false;
        // drop sample table, if it exists
        m_session << "DROP TABLE IF EXISTS Query", now;
        m_session << "CREATE TABLE Query (text TEXT)", now;

        if(m_thread.joinable() == false) {
            std::thread t(&Worker::threadFunc, this);
            m_thread = std::move(t);
        }
    }

    void stop() {
        m_stop_flag = true;
        if(m_thread.joinable()) m_thread.join();
    }

    void outStr(std::string&& buffer)
    {
	//--- вновь созданный узел станет фиктивным узлом
	StringNode* new_tail = new StringNode;
	new_tail->next = nullptr;
	//--- данные добавим перед новым хвостом
	std::lock_guard<std::mutex> tail_lock(m_tail_sync);
	m_tail->msg = std::move(buffer);
	m_tail->next = new_tail;
	m_tail = new_tail;
    }

private:
    StringNode* get_tail()
    {
	std::lock_guard<std::mutex> tail_lock(m_tail_sync);
	return m_tail;
    }

    StringNode* pop_head()
    {
	std::lock_guard<std::mutex> head_lock(m_head_sync);
	if(m_head == get_tail()) {
	    return nullptr;
	}
	StringNode* old_head = m_head;
	m_head = old_head->next;
	return old_head;
    }

    void threadFunc()
    {
        std::cout << "Thread func is started" << std::endl;

	while (true) {
	    StringNode* old_head = pop_head();
	    if (old_head) {
		Statement statement = (m_session << "INSERT INTO Query VALUES('" << old_head->msg << "')");
		std::cout << "toDB: " << statement.toString() << std::endl;
		statement.execute();
		delete old_head;
		continue;
	    }
	    //--- выходим если элементы кончились и сказали выходить
	    if (m_stop_flag == true) break;
	    //--- иначе соснем на 10 мс.
	    std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
        std::cout << "Thread func is stopped" << std::endl;
    }

private:

    StringNode*		m_head;
    StringNode*		m_tail;
    std::mutex		m_head_sync;
    std::mutex		m_tail_sync;
    std::thread		m_thread;
    std::atomic_int m_stop_flag;

    Poco::Data::Session m_session;
};

#endif //DEMO_SERVER_WORKER_H
