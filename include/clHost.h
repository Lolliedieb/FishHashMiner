// FishHash OpenCL Miner
// OpenCL Host Interface

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <climits>

#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_TARGET_OPENCL_VERSION 120

#include <CL/opencl.hpp>

using namespace std;

namespace fishHashMiner {

// Forward declaration for quicker compile time
class ironFishStratum;

class clHost {
	private:
	// OpenCL 
	vector<cl::Platform> platforms;  
	vector<cl::Context> contexts;
	vector<cl::CommandQueue> queues;
	vector<cl::Device> devices;
	vector<cl::Event> events;

	vector< vector<cl::Buffer> > buffers;
	vector< vector<cl::Kernel> > kernels;

	// To check if a mining thread stoped and we must resume it
	vector<bool> paused;
	
	// Pause function and statistics
	bool restart = true;
	vector<uint64_t> hashesCnt;

	// Functions
	void detectPlatformDevices(vector<int32_t>);
	void prepareDevice(cl::Device &, uint32_t, uint64_t, uint64_t);
	void gpuMainLoop(uint32_t);
	
	// The connector
	ironFishStratum* stratum;

	public:	
	void setup(ironFishStratum*, vector<int32_t>);
	void startMining();	
};

}
