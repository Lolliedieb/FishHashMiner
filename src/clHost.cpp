// FishHash OpenCL Miner
// OpenCL Host Interface

#include "clHost.h"
#include "FishHash.h"
#include "ironFishStratum.h"

namespace fishHashMiner {


/**************************************

	Helper Functions

**************************************/

// Helper functions to split a string
inline vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while(getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


inline vector<string> split(const string &s, char delim) {
    vector<string> elems;
    return split(s, delim, elems);
}


// Helper function that tests if a OpenCL device supports a certain CL extension
inline bool hasExtension(cl::Device &device, string extension) {
	string info;
	device.getInfo(CL_DEVICE_EXTENSIONS, &info);
	vector<string> extens = split(info, ' ');

	for (size_t i=0; i<extens.size(); i++) {
		if (extens[i].compare(extension) == 0) 	return true;
	} 
	return false;
}

/**************************************

	Function for OpenCL kernel building and on device memory allocation 

**************************************/

void clHost::prepareDevice(cl::Device &device, uint32_t platform, uint64_t dagSize, uint64_t lightSize) {
	cout << "   Loading and compiling FishHash OpenCL Kernel" << endl;

	// reading the kernel from file
	ifstream sourcefile("./kernels/fishhash.cl");
	cl::Program::Sources source;
	
	if (sourcefile) {
		string str((std::istreambuf_iterator<char>(sourcefile)), std::istreambuf_iterator<char>());
		source.push_back(str);
	}
		
	// Create a program object and build it
	vector<cl::Device> devicesTMP;
	devicesTMP.push_back(device);

	cl::Program program(contexts[platform], cl::Program::Sources(source));
	int32_t err = program.build(devicesTMP,"");
		
	// Check if the build was Ok
	if (!err) {
		cout << "   Build sucessfull. " << endl;

		// Store the device and create a queue for it
		cl_command_queue_properties queue_prop = 0;  
		devices.push_back(device);
		queues.push_back(cl::CommandQueue(contexts[platform], devices[devices.size()-1], queue_prop, NULL)); 

		// Reserve events, space for storing results and so on
		events.push_back(cl::Event());
		paused.push_back(true);
		hashesCnt.push_back(0);
		
		// Create the kernels
		vector<cl::Kernel> newKernels;	
		newKernels.push_back(cl::Kernel(program, "build", &err));
		newKernels.push_back(cl::Kernel(program, "mine", &err));
		kernels.push_back(newKernels);

		// Create the buffers
		vector<cl::Buffer> newBuffers;	
		
		newBuffers.push_back(cl::Buffer(contexts[platform], CL_MEM_READ_WRITE,   dagSize, NULL, &err));		// DAG
		newBuffers.push_back(cl::Buffer(contexts[platform], CL_MEM_READ_WRITE, lightSize, NULL, &err)); 	// Light Cache
		newBuffers.push_back(cl::Buffer(contexts[platform], CL_MEM_READ_WRITE, 192UL, NULL, &err)); 	// Header
		newBuffers.push_back(cl::Buffer(contexts[platform], CL_MEM_READ_WRITE,  40UL, NULL, &err)); 	// Results
		
		buffers.push_back(newBuffers);		
			
	} else {
		cout << "   Program build error, device will not be used. " << endl;
		// Print error msg so we can debug the kernel source
		cout << "   Build Log: "     << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devicesTMP[0]) << endl;
	}
}

/**************************************

	Function to read the OpenCl platforms on the system, pick the selected GPUs 
	and do a memory check if enough free memory is available on the GPUs 
	If passed the miner will initialize them for FishHash mining

**************************************/


void clHost::detectPlatformDevices(vector<int32_t> selDev) {

	const FishHash::fishhash_context* ctx = FishHash::get_context(false);
	
	uint64_t dagSize = ((uint64_t) ctx->full_dataset_num_items) * 128UL;
	uint64_t lightSize = ((uint64_t) ctx->light_cache_num_items) * 64UL;
	
	// This way we round up to a full MByte
	dagSize   =   (dagSize + 1048576) & 0xFFFFFFFFFFF00000UL; 
	lightSize = (lightSize + 1048576) & 0xFFFFFFFFFFF00000UL; 
	
	uint64_t minimalMemory = dagSize + lightSize + 268435456UL; // Add extra 256 MByte as safety margin

	// read the OpenCL platforms on this system
	cl::Platform::get(&platforms);  

	// this is for enumerating the devices
	int32_t curDev = 0;
	uint32_t selNum = 0;
	
	for (uint64_t pl=0; pl<platforms.size(); pl++) {
		// Create the OpenCL contexts, one for each platform
		cl_context_properties properties[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[pl](), 0};  
		cl::Context context;
		context = cl::Context(CL_DEVICE_TYPE_GPU, properties);
		contexts.push_back(context);

		// Read the devices of this platform
		vector< cl::Device > nDev = context.getInfo<CL_CONTEXT_DEVICES>();  
		for (uint32_t deviceIndex=0; deviceIndex<nDev.size(); deviceIndex++) {
			
			// Print the device name
			string name;
			nDev[deviceIndex].getInfo(CL_DEVICE_NAME, &name);
			
			if ( hasExtension(nDev[deviceIndex], "cl_amd_device_attribute_query") ) {
				string boardName;
				nDev[deviceIndex].getInfo(0x4038,&boardName);		// on AMD this often gives more readable result
				if (boardName.length() != 0) name = boardName;
			}

			// Get rid of strange characters at the end of device name
			if ((name.length() > 0) && (isalnum((int) name.back()) == 0)) {
				name.pop_back();
			} 	
			cout << "Found device " << curDev << ": " << name << endl;

			// Check if the device should be selected based on devices parameter
			bool pick = false;

			if (selDev[0] == -1) pick = true;
			if (selNum < (uint32_t) selDev.size()) {
				if (curDev == selDev[selNum]) {
					pick = true;
					selNum++;
				}				
			}
			
			if (pick) {
				// Check if the CPU / GPU has enough memory
				uint64_t deviceMemory = nDev[deviceIndex].getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();		
				cout << "   Device reports " << deviceMemory / (1024*1024) << " MByte total memory" << endl;
				
				if ( hasExtension(nDev[deviceIndex], "cl_amd_device_attribute_query") ) {
					uint64_t freeDeviceMemory;
				 	nDev[deviceIndex].getInfo(CL_DEVICE_GLOBAL_FREE_MEMORY_AMD, &freeDeviceMemory);  // CL_DEVICE_GLOBAL_FREE_MEMORY_AMD
					freeDeviceMemory *= 1024;
					cout << "   Device reports " << freeDeviceMemory / (1024*1024) << " MByte free memory (AMD)" << endl;
					deviceMemory = min<uint64_t>(deviceMemory, freeDeviceMemory);
				}
				
				if (deviceMemory > minimalMemory) {
					cout << "   Memory check passed" << endl;
					prepareDevice(nDev[deviceIndex], pl, dagSize, lightSize);
				} else {
					cout << "   Memory check failed, required minimum memory (MBytes): " << minimalMemory/(1024*1024) << endl;
				}
			} else {
				cout << "   Device will not be used, it was not included in --devices parameter." << endl;
			}

			curDev++; 
		}
	}

	if (devices.size() == 0) {
		cout << "No compatible OpenCL devices found or all are deselected. Closing fishHashMiner." << endl;
		exit(0);
	}
}

// Setup function called from outside
void clHost::setup(ironFishStratum* stratumIn, vector<int32_t> selectedDevices) {
	stratum = stratumIn;
	detectPlatformDevices(selectedDevices);
}




/**************************************

	Main functional loop for each GPU

**************************************/


// Function that will catch new work from the stratum interface and then queue the work on the device
void clHost::gpuMainLoop(uint32_t gpuIndex) {
	
	// Get the number of compute units - to make the work size a multiple of it
	uint32_t computeUnits;
	devices[gpuIndex].getInfo(CL_DEVICE_MAX_COMPUTE_UNITS,&computeUnits);
	
	// AMD RDNA+ cards report only half as many units as would the previous generations do
	if (hasExtension(devices[gpuIndex], "cl_amd_device_attribute_query")) {
		uint32_t arch;
		devices[gpuIndex].getInfo(CL_DEVICE_GFXIP_MAJOR_AMD, &arch);
		
		if (arch >= 10) {
			computeUnits *= 2;
		}
	}
	
	// Intel Arc GPUs report 8x as much CUs as other vendors would do
	if (hasExtension(devices[gpuIndex], "cl_intel_device_attribute_query")) {
		computeUnits /= 8;
	}
	
	uint32_t workSizeBuild = computeUnits*32*128;
	uint32_t workSizeMine = computeUnits*256*128;
	
	// Read the FishHash parameters	
	const FishHash::fishhash_context* ctx = FishHash::get_context(false);
	
	// Upload the light cache
	queues[gpuIndex].enqueueWriteBuffer(buffers[gpuIndex][1], CL_TRUE, 0, ctx->light_cache_num_items*64UL, ctx->light_cache);
	
	// Prepare for DAG generation
	kernels[gpuIndex][0].setArg(0, buffers[gpuIndex][0]); 		// DAG
	kernels[gpuIndex][0].setArg(1, buffers[gpuIndex][1]);		// Light Cache
	kernels[gpuIndex][0].setArg(2, ctx->full_dataset_num_items); 	// DAG Elements
	kernels[gpuIndex][0].setArg(3, ctx->light_cache_num_items);	// Light Cache Elements
	
	cout << "Device " << gpuIndex << ": starting dataset build" << endl;
	int32_t doneBuild=0;
	
	// Each thread in build kernel builds HALF a DAG item.
	while (doneBuild < 2*ctx->full_dataset_num_items) {
		kernels[gpuIndex][0].setArg(4, doneBuild);
		queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][0], cl::NDRange(0), cl::NDRange(4*workSizeBuild), cl::NDRange(128), NULL, NULL);
		queues[gpuIndex].finish();
		doneBuild += workSizeBuild;
	}
	
	cout << "Device " << gpuIndex << ": done dataset build" << endl;
	
	// Prepare the main mining loop
	kernels[gpuIndex][1].setArg(0, buffers[gpuIndex][0]);		// DAG
	kernels[gpuIndex][1].setArg(1, buffers[gpuIndex][2]);		// Block Header
	kernels[gpuIndex][1].setArg(2, buffers[gpuIndex][3]); 		// Solver Results
	kernels[gpuIndex][1].setArg(3, ctx->full_dataset_num_items); 	// DAG Elements
	
	// To check if work needs an update and if there is new stuff to submit
	vector<uint8_t> oldHeader;
	vector<uint8_t> oldResults(40, (uint8_t) 0);
	
	// Empty results buffer on GPU as well
	queues[gpuIndex].enqueueWriteBuffer(buffers[gpuIndex][3], CL_TRUE, 0, oldResults.size(), oldResults.data());
	
	// Main mining loop
	while (restart) {
		// Check if we have work to process
		if (stratum->hasWork()) {
			// Fetch work & start nonce from stratum interface
			workDescription wd = stratum->getWork(workSizeMine);
			
			// If the header changed, then upload the new one to GPU
			if (oldHeader != wd.header) {
				queues[gpuIndex].enqueueWriteBuffer(buffers[gpuIndex][2], CL_FALSE, 0, wd.header.size(), wd.header.data());
				oldHeader = wd.header;
			}
			
			// Set the start nonce and target
			kernels[gpuIndex][1].setArg(4, wd.startNonce);	// Start Nonce
			kernels[gpuIndex][1].setArg(5, wd.target);	// Target
			
			// Queue the mining kernel
			queues[gpuIndex].enqueueNDRangeKernel(kernels[gpuIndex][1], cl::NDRange(0), cl::NDRange(workSizeMine), cl::NDRange(128), NULL, NULL);
			//cout << err << endl;
			
			// Read the results (blocking / waiting read)
			vector<uint8_t> newResults(40, (uint8_t) 0);
			queues[gpuIndex].enqueueReadBuffer(buffers[gpuIndex][3], CL_TRUE, 0, newResults.size(), newResults.data());
			
			// See if there is anything to submit
			if (newResults != oldResults) {
				uint64_t * oldNonces = (uint64_t *) oldResults.data();
				uint64_t * newNonces = (uint64_t *) newResults.data();
				
				for (uint32_t i=1; i<5; i++) {
					if (oldNonces[i] != newNonces[i]) {
						stratum->handleSolution(wd, newNonces[i]);
					}
				}
				
				oldResults = newResults;
			}
			
			// Update counter for statistics
			hashesCnt[gpuIndex] += workSizeMine;
		} else {
			// If there is no work, we will pause this GPU until we got new again
			this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
}


void clHost::startMining() {

	// Start mining initially
	for (uint64_t i=0; i<devices.size(); i++) {	
		paused[i] = false;
		std::thread(&clHost::gpuMainLoop, this, i).detach();
	}


	// While the mining is running print some statistics
	while (restart) {
		this_thread::sleep_for(std::chrono::seconds(15));

		// Print performance stats (roughly)
		cout << "Performance: ";
		uint64_t totalHashes = 0;
		for (uint64_t i=0; i<devices.size(); i++) {
			uint64_t hashes = hashesCnt[i];
			hashesCnt[i] = 0;
			totalHashes += hashes;
			cout << fixed << setprecision(2) << (double) hashes / 15000000.0 << " Mh/s ";
			
		}

		if (devices.size() > 1) cout << "| Total: " << setprecision(2) << (double) totalHashes / 15000000.0 << " Mh/s";
		cout << endl;
	}
}


} 	// end namespace
