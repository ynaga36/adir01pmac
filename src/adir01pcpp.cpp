#include "adir01pcpp.hpp"

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <libusb-1.0/libusb.h>

#include <iostream>
#include <thread>

using namespace std;

//株式会社ビット・トレード・ワンの製品のUSB接続 赤外線リモコンアドバンス(製品型番: ADIR01P)用の送信設定アプリケーション v1.1のソースコードを参考にして実装。
/*
    ADIR01Pをいじってみてわかった事をここに記す。
    ファームウェアのソースコードは公開されていないので
    ここに書いてあることは正確ではないかもしれない。
    ちなみにリモコンから出力される赤外線は目では見えないが
    スマホなどのカメラを通してみることができる。

    ADIR01Pには赤外線を受信する受光部と
    指定した信号を発信できる赤外線LEDが備わっている。
    受光部で赤外線を受光すると緑色のLEDが発光する。
    (リモコンを使っていなくても時々そのLEDが発光するときがあるが)

    ADIR01P内には受信した信号を記録するバッファがある。
    バッファは16bit整数の配列になって、
    受光部が受光している時間としていない時間が交互に記録される。
    USBからreadStartReq命令を送るとバッファがクリアされて記録開始される。
    readStopReq命令を送ると記録が停止する。
    その後にreadDataGetReq命令を送るとバッファに記録された情報を取得できる。
    readStartReq命令を送った後はreadingDataGetReqで記録途中のバッファの内容を取得できる。

    readingDataGetReq命令を送っても読み取った分だけバッファからデータが減らされることはないようだ。
*/
namespace {
    const static uint16_t   idVendor        = 0x22ea;
    const static uint16_t   idProduct       = 0x003a;
    const static int        interfaceNum    = 3;
    const static size_t     PacketSize      = 64;
    const static uint8_t    EP_4_IN         = 0x84;
    const static uint8_t    EP_4_OUT        = 0x04;
    const static unsigned int usbTimeout    = 5000;

    //ADIR01Pを操作したりデータを取得するときにUSB経由で送る命令のコード
    //リモコンの赤外線信号を読み取りたいだけなら以下の順番で命令を送る。
    //readStartReq →  readStopReq →  readDataGetReq
    //またはreadStartReqとreadStopReqの間にreadingDataGetReqを繰り返し送って
    //totalSizeが0以上で同じ値が連続して続いた時は信号を読み取り終わったものとして
    //readStopReqを送る。
    //
    //readStartReqとreadStopReqの間に読み取り途中のデータが欲しいときはreadingDataGetReqを送る。
    //
    namespace deviceCmds {
        //ADIR01Pのファームウェアのバージョン取得
        const static uint8_t getFirmwareVersion = 0x56;
        //ADIR01PでIR読み取りを開始
        const static uint8_t readStartReq       = 0x31;
        //ADIR01PでIR読み取りを停止
        const static uint8_t readStopReq        = 0x32;
        //ADIR01PからIR読み取り中のデータを取得
        const static uint8_t readingDataGetReq  = 0x37;
        //ADIR01PからIR読み取り停止後に読み取ったデータを取得
        const static uint8_t readDataGetReq     = 0x33;
        //ADIR01Pから送信状態を取得
        const static uint8_t getSendStatusReq   = 0x38;
        //ADIR01Pに送信するIRデータを送る
        const static uint8_t setSendDataReq     = 0x34;
        //ADIR01PからIRデータを送信する
        const static uint8_t sendDataReq        = 0x35;
    }

    constexpr size_t sizeofParams() {
        return 0;
    }

    template<typename T, typename... Args>
    constexpr size_t sizeofParams(const T& t, const Args&... args) {
        return sizeof(t) + sizeofParams(args...);
    }

    void debugPrint(uint8_t* buf, size_t size) {
        clog << std::hex;
        clog << "Size: " << size << endl;
        for(size_t i=0; i<size; ++i) {
            clog << int(buf[i]) << ',';
            if((i+1) % 16 == 0)
                clog << endl;
        }
        if(size % 16 != 0)
            clog << endl;
    }

    bool enableDebugPrint = false;
    bool isDebugPrint() {
        return enableDebugPrint;
    }

    bool enableUSBIOPrint = false;
    bool isUSBIOPrint() {
        return enableUSBIOPrint;
    }
}

class libusbException : public std::runtime_error {
public:
    libusbException(libusb_error e):
        std::runtime_error(libusb_strerror(e)) {
    }

    libusbException(int e):
        libusbException(static_cast<libusb_error>(e)) {
    }
};

class adir01pcpp::adir01pcppImpl {
public:
    adir01pcppImpl() {
        if(libusb_init(&libusbContext) != 0)
            throw std::runtime_error("Failed to call libusb_init");
        libusb_device **devList;
        const auto numDevices = libusb_get_device_list(libusbContext, &devList);
        if(numDevices < 0)
            throw libusbException(numDevices);
        for(int i=0; i<numDevices; ++i) {
            auto const dev = devList[i];
            struct libusb_device_descriptor desc;
            libusb_get_device_descriptor(dev, &desc);
            if(desc.idVendor != idVendor || desc.idProduct != idProduct)
                continue;

            const auto ret = libusb_open(dev, &devHandle);
            if(ret < 0)
                throw libusbException(ret);
            break;
        }

        libusb_free_device_list(devList, 1);
        if(devHandle == NULL)
            throw std::runtime_error("ADIR01P was not found");
        {
            const auto ret = libusb_kernel_driver_active(devHandle,interfaceNum);
            if(ret == 1){
                const auto ret = libusb_detach_kernel_driver(devHandle, interfaceNum);
                if(ret != 0)
                    throw libusbException(ret);
            }else if(ret != 0)
                throw libusbException(ret);
        }

        {
            const auto ret = libusb_claim_interface(devHandle, interfaceNum);
            if(ret != 0)
                throw libusbException(ret);
        }
    }

    ~adir01pcppImpl() {
        if(devHandle != 0) {
            libusb_release_interface(devHandle, interfaceNum);
            libusb_close(devHandle);
        }
        libusb_exit(libusbContext);
    }

    std::string getFirmwareVersion() {
        deviceIO io(devHandle, deviceCmds::getFirmwareVersion);
        io.buffer[PacketSize-1] = 0;
        return std::string(reinterpret_cast<char*>(io.buffer + 1));
    }

    void readStartReq(uint16_t frequency) {
        if(isDebugPrint())
            clog << "readStartReq\n";

        deviceIO io(
            devHandle,
            deviceCmds::readStartReq, frequency,
            uint8_t(0),     // 読み込み停止フラグ　停止なし
            uint16_t(0),    // 読み込み停止ON時間
            uint16_t(0)     // 読み込み停止OFF時間
            );
    }

    void readStopReq() {
        if(isDebugPrint())
            clog << "readStopReq\n";

        deviceIO io(devHandle, deviceCmds::readStopReq);
        if(io.buffer[1] != 0) {
            throw std::runtime_error("Failed to read IR data");
        }
    }

    bool readingDataGetReq(IRData& irdata) {
        return getData(irdata, deviceCmds::readingDataGetReq);
    }

    bool readDataGetReq(IRData& irdata) {
        if(isDebugPrint())
            clog << "readDataGetReq\n";

        return getData(irdata, deviceCmds::readDataGetReq);
    }

    //falseなら未送信状態
    bool getSendStatusReq() {
        if(isDebugPrint())
            clog << "getSendStatus\n";

        deviceIO io(devHandle, deviceCmds::getSendStatusReq);

        size_t p = 2;
        return io.get<uint8_t>(p) != 0;
    }

    size_t setSendDataReq(const IRData& data, uint16_t pos) {
        const auto totalSize = uint16_t(data.size() / 4);
        const static uint8_t sizeMax = 0xe;
        const uint16_t sizeLeft = totalSize - pos;
        const uint8_t size = sizeLeft > sizeMax ? sizeMax : uint8_t(sizeLeft); 

        assert(pos < totalSize);

        deviceIO io(
            devHandle,
            deviceCmds::setSendDataReq,
            totalSize,
            pos,
            size,
            data.begin() + pos*4,
            data.begin() + (pos + size)*4
            );
        return pos + size;
    }

    void sendDataReq(uint16_t frequency, uint16_t size) {
        deviceIO io(
            devHandle,
            deviceCmds::sendDataReq,
            frequency,
            size);
    }

private:

    bool getData(IRData& irdata, uint8_t cmd) {
        deviceIO io(devHandle, cmd);
        size_t p = 1;
        const auto totalSize    = io.get<uint16_t>(p);
        if(totalSize == 0)
            return false;

        const auto startPos     = io.get<uint16_t>(p);
        const auto size         = io.get<uint8_t>(p);

        if(totalSize >= startPos + size && size > 0) {
            if(isDebugPrint()) {
                clog << "Copying IR Data(total:" << std::hex << totalSize << ", startPos: " << startPos << ", size: " << int(size) << ")" << endl;
            }
            for(size_t i=0; i<size*4; ++i)
                irdata.push_back(io.get<uint8_t>(p));
            return totalSize > startPos + size;
        }else
            return false;
    }

    struct deviceIO {
        template<typename... Args>
        deviceIO(struct libusb_device_handle *devHandle, uint8_t cmd, Args... args) {
            io(devHandle, cmd, args...);
        }

        template<typename T>
        T get(size_t& pos) const {
            T v = 0;
            for(size_t i=0; i<sizeof(T); ++i) {
                v <<= 8;
                v |= buffer[pos+i];
            }
            pos += sizeof(T);
            return v;
        }

        template<typename... Args>
        void io(struct libusb_device_handle *devHandle, uint8_t cmd, Args... args) {
            setCmd(cmd, args...);
            int transferred;
            {
                if(isUSBIOPrint()) {
                    clog << "Sending to USB\n";
                    debugPrint(buffer, PacketSize);
                }
                const auto ret = libusb_interrupt_transfer(devHandle, EP_4_OUT, buffer, PacketSize, &transferred, usbTimeout);
                if(ret < 0)
                    throw libusbException(ret);
                if(transferred != PacketSize)
                    throw std::runtime_error("Failed to send a packet to adir01p");
            }

            {
                const auto ret = libusb_interrupt_transfer(devHandle, EP_4_IN, buffer, PacketSize, &transferred, usbTimeout);
                if(ret < 0)
                    throw libusbException(ret);
                if(transferred != PacketSize)
                    throw std::runtime_error("Failed to receive a packet from adir01p");
            }

            if(buffer[0] != cmd)
                throw std::runtime_error("Failed to execute comand to adir01p");
            if(isUSBIOPrint()) {
                clog << "Received from USB\n";
                debugPrint(buffer, PacketSize);
            }
        }

        uint8_t buffer[PacketSize];

    private:
        template<typename... Args>
        void setCmd(uint8_t cmd, Args... args) {
            write(0, cmd, args...);
        }

        void write(size_t pos) {
            assert(pos < PacketSize);
            //データがUSBケーブルを通ったときにEMIや消費電力を低くするために0xffを書き込んでおいたほうがいいらしい。
            for(size_t i=pos; i<PacketSize; ++i) {
                buffer[i] = 0xff;
            }
        }

        template<typename... Args>
        void write(
            size_t pos,
            typename IRData::const_iterator begin,
            typename IRData::const_iterator end,
            Args... args
        ) {
            if(begin == end) {
                write(pos, args...);
            }else{
                auto next = begin;
                ++next;
                write(pos, *begin, next, end, args...);
            }
        }

        template<typename T, typename... Args>
        void write(size_t pos, T v, Args... args) {
            static_assert(sizeofParams(v, args...) <= PacketSize, "Exceeding packet size");
            T t = v;
            for(int i=sizeof(v); i>0; --i) {
                buffer[pos+i-1] = t;
                t >>= 8;
            }
            write(pos+sizeof(v), args...);
        }
    };

    libusb_context* libusbContext;
    struct libusb_device_handle *devHandle = NULL;
};

adir01pcpp::adir01pcpp():
    impl(std::make_unique<adir01pcppImpl>())
{
}

adir01pcpp::~adir01pcpp()
{
}

std::string adir01pcpp::getFirmwareVersion()
{
    return impl->getFirmwareVersion();
}

adir01pcpp::IRData adir01pcpp::readIRData(uint16_t frequency) {
    impl->readStartReq(frequency);
    this_thread::sleep_for(5s);
    impl->readStopReq();

    IRData      sample;
    while(impl->readDataGetReq(sample));
    if(!sample.empty()) {
        if(isDebugPrint())
            debugPrint(sample.data(), sample.size());
    }

    return sample;
}

void adir01pcpp::sendIR(const adir01pcpp::IRData& data, uint16_t frequency) {
    for(int i=0; i<5 && impl->getSendStatusReq(); ++i) {
        this_thread::sleep_for(100ms);
    }
    if(impl->getSendStatusReq())
        throw std::runtime_error("adir01p is not ready to transmit IR");

    size_t pos = 0;
    do {
        pos = impl->setSendDataReq(data, pos);
    } while(pos*4 < data.size());

    impl->sendDataReq(frequency, data.size()/4);
}

void adir01pcpp::readStart(uint16_t frequency) {
    impl->readStartReq(frequency);
}

adir01pcpp::IRData adir01pcpp::getReadingData() {
    IRData sample;
    while(impl->readingDataGetReq(sample)){}

    return sample;
}

void adir01pcpp::readStop() {
    impl->readStopReq();
}

adir01pcpp::IRData adir01pcpp::getReadData() {
    IRData      sample;
    while(impl->readDataGetReq(sample)){}

    return sample;
}

bool adir01pcpp::checkFrequency(uint16_t frequency) noexcept {
    return frequency >= frequencyMin && frequency <= frequencyMax;
}

void adir01pcpp::enableDebugPrint() noexcept {
    ::enableDebugPrint = true;
}

void adir01pcpp::enableUSBIOPrint() noexcept {
    ::enableUSBIOPrint = true;
}

void adir01pcpp::printIRData(std::ostream& ost, const IRData& data) {
    ost << std::hex;
    ost << "Size: " << data.size() << endl;
    const size_t size = data.size() / 2;
    size_t i = 0;
    for(; i<size; ++i) {
        uint16_t v = data[i*2] << 8 | data[i*2+1];
        ost << v << ',';
        if((i+1) % 16 == 0)
            ost << endl;
    }
    if(i % 16 != 0)
        ost << endl;
}

