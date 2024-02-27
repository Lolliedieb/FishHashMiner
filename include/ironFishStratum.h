// Fishhash OpenCL Miner
// Stratum Interface Class

#ifndef ironFishStratum_H 
#define ironFishStratum_H 

#include <iostream>
#include <thread>
#include <cstdlib>
#include <string>
#include <sstream>
#include <vector>
#include <deque>
#include <random>

#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <boost/scoped_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace std;
using namespace boost::asio;
using boost::asio::ip::tcp;
namespace pt = boost::property_tree;

namespace fishHashMiner {

struct workDescription {	
	vector<uint8_t> header;
	uint64_t startNonce;
	uint64_t target;
		
	string workId;
};

class ironFishStratum {
	public:
	ironFishStratum(string, string, string, string, string, bool);
	void startWorking();

	bool hasWork();
	workDescription getWork(uint64_t);
	void handleSolution(workDescription, uint64_t);

	private:

	// Definitions belonging to the physical connection
	boost::asio::io_service io_service;
	boost::scoped_ptr< tcp::socket > socket;
	tcp::resolver res;
	boost::asio::streambuf requestBuffer;
	boost::asio::streambuf responseBuffer;

	// User Data
	string host;
	string port;
	string user;
	string pass;
	string graffiti;
	bool debug = false;

	// Storage for received work
	workDescription work;
	vector<uint8_t> extraNonce;
	std::atomic<uint64_t> nonce;

	// Stat
	uint64_t sharesAcc = 0;
	uint64_t sharesRej = 0;
	time_t t_start, t_current;

	//Stratum sending subsystem
	bool activeWrite = false;
	void queueDataSend(string);
	void syncSend(string);
	void activateWrite();
	void writeHandler(const boost::system::error_code&);	
	std::deque<string> writeRequests;

	// Stratum receiving subsystem
	void readStratum(const boost::system::error_code&);
	boost::mutex updateMutex;

	// Connection handling
	void connect();
	void handleConnect(const boost::system::error_code& err,  tcp::resolver::iterator);	
};

// A helper function to parse the json tree more efficiently
template <typename T = std::string> 
T element_at(pt::iptree const& tree, std::string name, size_t n) {
    auto r = tree.get_child(name).equal_range("");

    for (; r.first != r.second && n; --n) ++r.first;

    if (n || r.first==r.second)
        throw std::range_error("index out of bounds");

    return r.first->second.get_value<T>();
}

#endif 

}

