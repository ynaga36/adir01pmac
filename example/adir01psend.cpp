#include "adir01pcpp.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

using namespace std;

int read(ostream& ost) {
    adir01pcpp  device;
    auto data = device.readIRData();
    if(data.empty()) {
        cerr << "No valid signal received\n";
        return 1;
    }
#if 0
    adir01pcpp::printIRData(cout, data);
#endif

    ost << hex;
    ost << adir01pcpp::frequencyDefault << '\n';

    size_t c = 0;
    for(const auto v : data) {
        c++;
        ost.width(2*sizeof(data[0]));
        ost << int(v) << ((c&0xf) == 0 ? '\n' : ' ');
        if(ost.fail()) {
            throw runtime_error("Error while writing IR data\n");
        }
    }

    ost.flush();
    if(ost.fail())
        throw runtime_error("Error while writing IR data\n");

    clog << "Received IR data has been written\n";

    return 0;
}

int read(int argc, char** argv) {
    if(argc > 2) {
        if(argv[2] == string("help")) {
            cerr << "Usage: " << argv[0] << " " << argv[1] << " [FILE]\n"
                 << "Read IR data and write it to specified file or stdout if no filename is specified\n";
            return 0;
        }else {
            if(argv[2][0] != '-') {
                ofstream ofs(argv[2]);
                return read(ofs);
            }
        }
    }

    return read(cout);
}

int send(istream& istrm, adir01pcpp& device) {
    istrm >> hex;
    uint16_t frequency;
    istrm >> frequency;
    if(istrm.fail())
        throw runtime_error("Invalid input");

    if(!adir01pcpp::checkFrequency(frequency))
        throw runtime_error("Unsupported frequency");

    adir01pcpp::IRData  data; 
    int c;
    while(istrm >> c) {
        if(c > 0xff)
            throw runtime_error("Invalid input");
        data.push_back(static_cast<adir01pcpp::IRData::value_type>(c));
    }

    if(!istrm.eof())
        throw runtime_error("Invalid input");

    device.sendIR(data, frequency);

    clog << "IR data has been sent\n";

    return 0;
}

int send(int argc, char** argv) {
    if(argc > 2) {
        if(argv[2] == string("help")) {
            cerr << "Usage: " << argv[0] << ' ' << argv[1] << " [FILE]...\n"
                 << "Read IR data from specified file/files or stdin and transmit them from the ADIR01P\n";
            return 0;
        }
    }

    adir01pcpp device;

    if(argc == 2)
        return send(cin, device);

    unsigned int wait = 0;
    bool isFirst = true;
    for(int i=2; i<argc; ++i) {
        if(argv[i] == string("-w")) {
            if(i == argc - 1)
                throw runtime_error("Specify waiting time in milliseconds");

            try {
                wait = stoul(argv[i+1]);
            }catch(const invalid_argument& e) {
                throw runtime_error("Invalid argument was given to -w option");
            }
            ++i;
            continue;
        }

        if(!isFirst && wait > 0)
            this_thread::sleep_for(chrono::milliseconds(wait));
        isFirst = false;
        ifstream ifs(argv[i]);
        if(!ifs.good()) {
            cerr << "Failed to open file: " << argv[i] << endl;
            return -1;
        }
        const auto ret = send(ifs, device);
        if(ret != 0) {
            cerr << "Failed to transmit file: " << argv[i] << endl;
            return ret;
        }
    }

    if(!isFirst)
        clog << "All IR data have been sent\n";
    return 0;
}

int printFWVer() {
    adir01pcpp  device;
    const auto ver = device.getFirmwareVersion();
    cout << "Firmware version is " << ver << endl;

    return 0;
}

int main(int argc, char** argv) {
    int ret = -1;
    try {
//        adir01pcpp::enableDebugPrint();
//        adir01pcpp::enableUSBIOPrint();

        bool printUsage = false;
        if(argc > 1) {
            if(string("r") == argv[1]) {
                ret = read(argc, argv);
            } else if(string("s") == argv[1]) {
                ret = send(argc, argv);
            } else if(string("v") == argv[1]) {
                ret = printFWVer();
            } else {
                printUsage = true;
            }
        } else {
            printUsage = true;
        }

        if(printUsage) {
            cerr << "Usage: " << argv[0] << " {COMMAND [help] | help}\n"
                 << "COMMAND\n"
                 << "    r    Read IR data and write to file or stdout\n"
                 << "    s    Send IR data from file or stdin\n"
                 << "    v    Show firmware version of the ADIR01P\n";
        }
    }catch(const exception& e){
        cerr << e.what() << endl;
        return -1;
    }catch(...) {
        cerr << "Unknown exception\n";
        return -1;
    }

    return ret;
}

