#ifndef MSDX_HH
#define MSDX_HH

#include "MSXDevice.hh"
#include "Rom.hh"
#include <memory>
#include <dirent.h>

namespace openmsx {

class msdx final : public MSXDevice
{
public:
	explicit msdx(const DeviceConfig& config);
	~msdx();

	void reset(EmuTime::param time) override;

	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;

        byte peekIO(word port, EmuTime::param time) const override;
        byte readIO(word port, EmuTime::param time) override;
        void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void changeControl(byte value, EmuTime::param time);
	void hexdump(int offset, int count);

	void strupper(char* dest);
	void strvalidate(char* p);
	int dirHandler();
	int dirBegin();

	Rom rom;

	FILE* imgs[2];
	bool changed[2];
	int currentDrive;
	long logicalSector;

	std::string home;

	unsigned char ioBuffer[512];
	unsigned char lastread;
	unsigned char iodata;
	unsigned char portd;
	unsigned char status;
	int bp;
	int mode;

	word dataReg;
	byte controlReg;

	int dirState;
	DIR* dir;
};

} // namespace openmsx

#endif
