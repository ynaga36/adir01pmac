#include "adir01pcpp.hpp"
#include <vector>
#include <iostream>
#include <chrono>

using namespace std;

int main(int argc, char** argv) {
    try {
        typedef std::vector<adir01pcpp::IRData> IRSamples;

//        adir01pcpp::enableDebugPrint();

        adir01pcpp device;
        device.readStart();
        const auto begin = chrono::system_clock::now();

        IRSamples   samples;
        do {
            auto sample = device.getReadingData();
            if(sample.empty())
                continue;

            samples.emplace_back(move(sample));
        } while(samples.size() < 256 && chrono::system_clock::now() - begin < 4s);

        device.readStop();

        auto sample = device.getReadData();
        if(!sample.empty())
            samples.emplace_back(move(sample));

        if(samples.empty()) {
            cerr << "No valid signal received\n";
            return 1;
        }

        cout << "Got " << std::dec << samples.size() << " samples\n";

        cout << "List of size of each sample:\n";
        for(const auto& s : samples)
            cout << s.size() << ", ";
        cout << endl;

        for(const auto& s : samples)
            adir01pcpp::printIRData(cout, s);
    }catch(const exception& e){
        cout << e.what() << endl;
        return 1;
    }

    return 0;
}
