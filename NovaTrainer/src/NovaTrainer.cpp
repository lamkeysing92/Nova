//============================================================================
// Name        : NovaTrainer.cpp
// Copyright   : DataSoft Corporation 2011-2012
//	Nova is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   Nova is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with Nova.  If not, see <http://www.gnu.org/licenses/>.
// Description : Command line interface for Nova
//============================================================================

#include "ClassificationEngine.h"
#include "EvidenceTable.h"
#include "SuspectTable.h"
#include "NovaTrainer.h"
#include "Evidence.h"
#include "Logger.h"

#include <netinet/if_ether.h>
#include <unistd.h>
#include <vector>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <fstream>
#include <sstream>
#include <iostream>


using namespace std;
using namespace Nova;


// Maintains a list of suspects and information on network activity
SuspectTable suspects;
//Contains packet evidence yet to be included in a suspect

ClassificationEngine *engine;
EvidenceTable suspectEvidence;

string trainingCapFile;

string pcapFile;
string haystackFile;
string localFile;

ofstream trainingFileStream;
pcap_t *handle;



double lastPoint[DIM];

int main(int argc, const char *argv[])
{
	if(argc < 3)
	{
		PrintUsage();
		return 0;
	}

	for(int i = 0; i < DIM; i++)
	{
		lastPoint[i] = -1;
	}

	Config::Inst();
	Config::Inst()->SetIsDmEnabled(false);

	if(chdir(Config::Inst()->GetPathHome().c_str()) == -1)
	{
		LOG(CRITICAL, "Unable to change folder to " + Config::Inst()->GetPathHome(), "");
	}

	pcapFile = string(argv[1]) + "/capture.pcap";
	haystackFile = string(argv[1]) + "/haystackIps.txt";
	localFile = string(argv[1]) + "/localIps.txt";

	UpdateHaystackFeatures();

	engine = new ClassificationEngine(suspects);
	engine->LoadDataPointsFromFile(Config::Inst()->GetPathTrainingFile());



	trainingCapFile = argv[2];

	// We suffix the training capture files with the date/time
	char errbuf[PCAP_ERRBUF_SIZE];
	struct bpf_program fp;

	trainingFileStream.open(trainingCapFile.data(), ios::app);
	if(!trainingFileStream.is_open())
	{
		LOG(CRITICAL, "Unable to open the training capture file.", "Unable to open training capture file at: "+trainingCapFile);
	}

	handle = pcap_open_offline(pcapFile.c_str(), errbuf);
	if(handle == NULL)
	{
		LOG(CRITICAL, "Unable to start packet capture.", "Couldn't open pcap file: "+pcapFile+": "+string(errbuf)+".");
	}

	if(pcap_compile(handle, &fp, "not src host 0.0.0.0", 0, PCAP_NETMASK_UNKNOWN) == -1)
	{
		LOG(CRITICAL, "Unable to start packet capture.", string("Couldn't parse filter: ") + pcap_geterr(handle) +".");
	}

	if(pcap_setfilter(handle, &fp) == -1)
	{
		LOG(CRITICAL, "Unable to start packet capture.", string("Couldn't install filter: ") + pcap_geterr(handle) +".");
	}

	pcap_freecode(&fp);
	//First process any packets in the file then close all the sessions
	pcap_dispatch(handle, -1, Nova::HandleTrainingPacket,NULL);

	trainingFileStream.close();
	LOG(DEBUG, "Done processing PCAP file", "");

	pcap_close(handle);

	return EXIT_SUCCESS;
}

namespace Nova
{

void PrintUsage()
{
	cout << "Usage:" << endl;
	cout << "  novatrainer novaCaptureFolder trainingOutput.dump" << endl;
	cout << endl;

	exit(EXIT_FAILURE);
}

void HandleTrainingPacket(u_char *index,const struct pcap_pkthdr *pkthdr,const u_char *packet)
{

	if(packet == NULL)
	{
		LOG(ERROR, "Failed to capture packet!","");
		return;
	}
	switch(ntohs(*(uint16_t *)(packet+12)))
	{
		//IPv4, currently the only handled case
		case ETHERTYPE_IP:
		{
			//Prepare Packet structure
			Evidence *evidencePacket = new Evidence(packet + sizeof(struct ether_header), pkthdr);
			suspects.ProcessEvidence(evidencePacket);
			update(evidencePacket->m_evidencePacket.ip_src);

			return;
		}
		default:
		{
			//stringstream ss;
			//ss << "Ignoring a packet with unhandled protocol #" << (uint16_t)(ntohs(((struct ether_header *)packet)->ether_type));
			//LOG(DEBUG, ss.str(), "");
			return;
		}
	}
}

void update(const in_addr_t& key)
{
	in_addr_t foo = key;
	suspects.ClassifySuspect(foo);

	//Check that we updated correctly
	Suspect suspectCopy = suspects.GetSuspect(foo);

	if(suspects.IsEmptySuspect(&suspectCopy))
	{
		cout << "Got an empty suspect when trying to classify" << endl;
		return;
	}

	//Store in training file if needed
	trainingFileStream << suspectCopy.GetIpString() << " ";

	FeatureSet fs = suspectCopy.GetFeatureSet(MAIN_FEATURES);
	if(fs.m_features[0] != fs.m_features[0] )
	{
		cout << "This can't be good..." << endl;
	}
	for(int j = 0; j < DIM; j++)
	{
		trainingFileStream << fs.m_features[j] << " ";
	}
	trainingFileStream << "\n";
}


void UpdateHaystackFeatures()
{
	vector<string> haystackAddresses = Config::GetHaystackAddresses(haystackFile);

	vector<uint32_t> haystackNodes;
	for(uint i = 0; i < haystackAddresses.size(); i++)
	{
		cout << haystackAddresses[i] << " has been set as a haystack address" << endl;
		haystackNodes.push_back(htonl(inet_addr(haystackAddresses[i].c_str())));
	}

	suspects.SetHaystackNodes(haystackNodes);
}

}
