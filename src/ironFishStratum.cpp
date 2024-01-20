// FishHash OpenCL Miner
// Stratum interface class

#include "ironFishStratum.h"
#include "FishHash.h"

namespace fishHashMiner {

// This one ensures that the calling thread can work on immediately
void ironFishStratum::queueDataSend(string data) {
	io_service.post(boost::bind(&ironFishStratum::syncSend,this, data)); 
}

// Function to add a string into the socket write queue
void ironFishStratum::syncSend(string data) {
	writeRequests.push_back(data);
	activateWrite();
}


// Got granted we can write to our connection, lets do so	
void ironFishStratum::activateWrite() {
	if (!activeWrite && writeRequests.size() > 0) {
		activeWrite = true;

		string json = writeRequests.front();
		writeRequests.pop_front();

		std::ostream os(&requestBuffer);
		os << json;
		if (debug) cout << "Write to connection: " << json;

		boost::asio::async_write(*socket, requestBuffer, boost::bind(&ironFishStratum::writeHandler,this, boost::asio::placeholders::error)); 		
	}
}
	

// Once written check if there is more to write
void ironFishStratum::writeHandler(const boost::system::error_code& err) {
	activeWrite = false;
	activateWrite(); 
	if (err) {
		if (debug) cout << "Write to stratum failed: " << err.message() << endl;
	} 
}


// Called by main() function, starts the stratum client thread
void ironFishStratum::startWorking(){
	t_start = time(NULL);
	std::thread (&ironFishStratum::connect,this).detach();
}

// This function will be used to establish a connection to the API server
void ironFishStratum::connect() {	
	while (true) {
		tcp::resolver::query q(host, port); 

		cout << "Connecting to " << host << ":" << port << endl;
		try {
	    		tcp::resolver::iterator endpoint_iterator = res.resolve(q);
			tcp::endpoint endpoint = *endpoint_iterator;
			socket.reset(new tcp::socket(io_service));

			socket->async_connect(endpoint,
			boost::bind(&ironFishStratum::handleConnect, this, boost::asio::placeholders::error, ++endpoint_iterator));	

			io_service.run();
		} catch (std::exception const& _e) {
			 cout << "Stratum error: " <<  _e.what() << endl;
		}

		work.workId = "-1";
		io_service.reset();
		socket->close();

		cout << "Lost connection to IronFish stratum server" << endl;
		cout << "Trying to connect in 5 seconds"<< endl;

		std::this_thread::sleep_for(std::chrono::seconds(5));
	}		
}


// Once the physical connection is there listen and send subscription request
void ironFishStratum::handleConnect(const boost::system::error_code& err, tcp::resolver::iterator endpoint_iterator) {
	if (!err) {
		cout << "Connected to pool." << endl;

	      	// The connection was successful. Listen to incomming messages
		boost::asio::async_read_until(*socket, responseBuffer, "\n",
		boost::bind(&ironFishStratum::readStratum, this, boost::asio::placeholders::error));

		// The connection was successful. Send the subscribe request
		std::stringstream json;
		json << "{\"id\":0,\"method\":\"mining.subscribe\",\"body\":{\"version\":2,\"agent\":\"fishHash reference miner\",\"publicAddress\":\"" << user << "\",\"extend\":[\"mining.submitted\"]}}\n";
		
		queueDataSend(json.str());	
			
    	} else if (err != boost::asio::error::operation_aborted) {
		if (endpoint_iterator != tcp::resolver::iterator()) {
			// The endpoint did not work, but we can try the next one
			tcp::endpoint endpoint = *endpoint_iterator;

			socket->async_connect(endpoint,
			boost::bind(&ironFishStratum::handleConnect, this, boost::asio::placeholders::error, ++endpoint_iterator));
		} 
	} 	
}



// Simple helper function that casts a hex string into byte array
vector<uint8_t> parseHex (string input) {
	vector<uint8_t> result ;
	result.reserve(input.length() / 2);
	for (uint32_t i = 0; i < input.length(); i += 2){
		uint32_t byte;
		std::istringstream hex_byte(input.substr(i, 2));
		hex_byte >> std::hex >> byte;
		result.push_back(static_cast<unsigned char>(byte));
	}
	return result;
}


// Main stratum read function, will be called on every received data
void ironFishStratum::readStratum(const boost::system::error_code& err) {
	if (!err) {
		// We just read something without problem.
		std::istream is(&responseBuffer);
		std::string response;
		getline(is, response);

		if (debug) cout << "Incoming Stratum: " << response << endl;

		// Parse the input to a property tree
		pt::iptree jsonTree;
		try {
			istringstream jsonStream(response);
			pt::read_json(jsonStream,jsonTree);

			// This data will help getting the parsing right
			//int id = jsonTree.get<int>("id", -1);
			string method = jsonTree.get<string>("method", "null");
			
			// If id >= 0 it is a reply to a send message

			// Parse subscribe result
			if (method.compare("mining.subscribed") == 0) {
				int exists = jsonTree.count("body");

				if (exists > 0) {
					// Read the extranonce					
					string nonceStr = jsonTree.get<string>("body.xn", ""); //element_at<string>(jsonTree, "result", 1);
					extraNonce = parseHex(nonceStr);
					
					cout << "Miner subscribed to pool, extra nonce: " <<  nonceStr  << endl;
				}	
			}
			
			// Read a new target
			if (method.compare("mining.set_target") == 0) {
				int exists = jsonTree.count("body");
	
        			if (exists > 0) {
        				string tarStr = jsonTree.get<string>("body.target");
        				vector<uint8_t> tarHex = parseHex(tarStr);
        				
					uint64_t target = 0;
					for (int32_t i=0; i<8; i++) {
						target *= 256;
						target += (uint64_t) tarHex[i];
					}
					
					work.target = target;
					cout << "New Target received: " << tarStr.substr(0,16) << endl; 	
        			}
			}
			
			// Read a new extra nonce (this should work with NiceHash)
			if (method.compare("mining.set_target") == 0) {
				int exists = jsonTree.count("params");
				if (exists > 0) {
					string nonceStr = element_at<string>(jsonTree, "params", 0);
					extraNonce = parseHex(nonceStr);
					cout << "New extra nonce received: " <<  nonceStr  << endl;
				}
			}

			// We got a new block header / work
			if (method.compare("mining.notify") == 0) {
				int exists = jsonTree.count("body");
				if (exists > 0) {					
			    		work.workId = jsonTree.get<string>("body.miningRequestId");   
			    		work.header = parseHex(jsonTree.get<string>("body.header") + "000000000000000000000000");	// Expect 180 bytes in - that we pad to 192 (=3*64) bytes for Blake3 in the kernel code
			    		
			    		cout << "New job with id " << work.workId << " received" << endl;
			    	}
			}

			// Count a share response
			if (method.compare("mining.submitted") == 0) {
				int exists = jsonTree.count("body");
				if (exists > 0) {
					bool accepted = jsonTree.get<bool>("body.result", false);
					
					if (accepted) {
						sharesAcc++;
						cout << "Share accepted" << endl;
					} else {
						sharesRej++;
						cout << "Share not accepted" << endl;
					}
					
					t_current = time(NULL);
					cout << "Shares (A/R): " << sharesAcc << " / " << sharesRej << " Uptime: " << (int)(t_current-t_start) << " sec" << endl;
				}
			}
			

		} catch(const pt::ptree_error &e) {
			cout << "Json parse error: " << e.what() << endl; 
			
		}

		// Prepare to continue reading
		boost::asio::async_read_until(*socket, responseBuffer, "\n",
        	boost::bind(&ironFishStratum::readStratum, this, boost::asio::placeholders::error));
	}
}

// Checking if we have valid work, else the GPUs will pause
bool ironFishStratum::hasWork() {
	return (work.workId.compare("-1") != 0);
}

// function the clHost class uses to fetch new work
workDescription ironFishStratum::getWork(uint64_t neededNonces) {

	// Generate the next startNonce where we are sure to have no overflow into the extra nonce bytes
	uint64_t startNonce;	
	bool passed = false;	
	do {
		startNonce = nonce.fetch_add(neededNonces);
		uint8_t * noncePoint = (uint8_t *) &startNonce;

		for (uint32_t i=0; i<extraNonce.size(); i++) {
			noncePoint[7-i] = extraNonce[i];
		}
		
		uint64_t check = startNonce+neededNonces;
		uint8_t * noncePointB = (uint8_t *) &check;
		passed = true;
		
		for (uint32_t i=0; i<extraNonce.size(); i++) {
			passed &= (noncePointB[7-i] == noncePoint[7-i]);
		}
	} while (!passed);
	
	// Copy the work and the found nonce for the solver
	workDescription ret = work;
	ret.startNonce = startNonce;
	
	return ret;
}


// Will be called by clHost class for check & submit
void ironFishStratum::handleSolution(workDescription wd, uint64_t nonce) {

	// With the future block header format this might need an amendment to the right position
	((uint64_t *) wd.header.data())[0] = nonce;	
	
	// We perform a CPU check to see the hash validity
	vector<uint8_t> cpuHash;
	cpuHash.assign(32, 0);
	
	const FishHash::fishhash_context* ctx = FishHash::get_context(false);
	FishHash::hash(cpuHash.data(), ctx, wd.header.data(), 180);
	
	// Convert the endianes
	uint64_t hashValue;
	for (uint32_t a = 0; a < 8; a++) {
		((uint8_t *) &hashValue)[7-a] = cpuHash[a];
	}
		
	// Check if we are fine and submit
	if (hashValue <= wd.target) {
		std::stringstream nonceStr;
		for (int32_t i=0; i<8; i++) {
			nonceStr << std::setfill('0') << std::setw(2) << std::hex << (unsigned) wd.header[i];
		}
		
		std::stringstream graffitiStr;
		for (int32_t i=0; i<32; i++) {
			graffitiStr << std::setfill('0') << std::setw(2) << std::hex << (unsigned) wd.header[148+i];
		}
		
		std::stringstream json;
		json << "{\"id\":1,\"method\":\"mining.submit\",\"body\":{\"miningRequestId\":" << wd.workId << ",\"randomness\":\"" << nonceStr.str() << "\",\"graffiti\":\"" << graffitiStr.str() << "\"}}\n";
		queueDataSend(json.str());
	}	
}


ironFishStratum::ironFishStratum(string hostIn, string portIn, string userIn, string passIn, bool debugIn) : res(io_service) {

	host = hostIn;
	port = portIn;
	user = userIn;
	pass = passIn;
	debug = debugIn;

	// Set defaults for work
	work.target = 0;
	work.workId = "-1";
	
	// We pick a random start nonce
	random_device rd;
	default_random_engine generator(rd());
	uniform_int_distribution<uint64_t> distribution(0,0xFFFFFFFFFFFFFFFFUL);
	nonce = distribution(generator);

}

} // End namespace

