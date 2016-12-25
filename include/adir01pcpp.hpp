#pragma once
#include <memory>
#include <string>
#include <vector>

class adir01pcpp {
public:
    typedef std::vector<uint8_t> IRData;

    const static uint16_t frequencyMin      = 25000;
    const static uint16_t frequencyMax      = 50000;
    const static uint16_t frequencyDefault  = 38000;

    adir01pcpp();
    ~adir01pcpp();

    std::string getFirmwareVersion();
    //これを呼んでから受光部に信号を送ると読み取ったデータが返る。
    IRData readIRData(uint16_t frequency = frequencyDefault);
    //readIRDataで得た赤外線データを送信する。
    void sendIR(const IRData& data, uint16_t frequency = frequencyDefault);

    //受光部で読み取ったデータをリアルタイムに取得したいときには以下のメンバ関数を使う。
    void readStart(uint16_t frequency = frequencyDefault);
    //これはreadStartを呼んだ後, readStopを呼ぶ前に呼ぶ。
    IRData getReadingData();
    //readStartとgetReadingDataの後に呼ぶ。
    void readStop();
    //readStartしてからreadStopするまでに読み取った赤外線データを返す。
    IRData getReadData();

    static bool checkFrequency(uint16_t frequency) noexcept;

    static void enableDebugPrint() noexcept;
    static void enableUSBIOPrint() noexcept;
    static void printIRData(std::ostream& ost, const IRData& data);

private:
    class adir01pcppImpl;

    std::unique_ptr<adir01pcppImpl> impl;
};
