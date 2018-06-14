#ifndef MSDX_HH
#define MSDX_HH

#include "MSXDevice.hh"
#include "Rom.hh"
#include <memory>

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

	Rom rom;
	FILE* userFile;
	long logicalSector;
	unsigned char ioBuffer[512];
	unsigned char lastread;
	unsigned char iodata;
	unsigned char portd;
	unsigned char status;
	int bp;
	int mode;

	word dataReg;
	byte controlReg;
};

} // namespace openmsx

#endif
