#include "msdx.hh"
#include "IDEDevice.hh"
#include "serialize.hh"

#include <string>
#include <iostream>

using std::string;


namespace openmsx {

msdx::msdx(const DeviceConfig& config)
	: MSXDevice(config)
	, rom(getName() + " ROM", "rom", config)
{
	powerUp(getCurrentTime());
}

msdx::~msdx() = default;

void msdx::reset(EmuTime::param time)
{
	mode = 1;
	status = 0;
	userFile = NULL;
}

byte msdx::readMem(word address, EmuTime::param /*time*/)
{
	if (0x4000 <= address && address < 0x8000) {
		return rom[address & 0x3FFF];
	}
	return 0xFF;
}

const byte* msdx::getReadCacheLine(word start) const
{
	if (0x4000 <= start && start < 0x8000) {
		return &rom[start & 0x3FFF];
	}
	return unmappedRead;
}

byte msdx::readIO(word port, EmuTime::param time)
{
	port &= 0x03;

	if (port == 0) {
		return status;
	}
	else if (port == 1) {
		portd = iodata;
		if (mode == 0) {
			iodata = ioBuffer[bp];
			bp = (bp + 1) & 511;
		}
		return portd;
	}
	else if (port == 3) {
		return 0x5D;
	}

	return 0xff;
}

byte msdx::peekIO(word port, EmuTime::param time) const
{
	port &= 0x03;

	if (port == 0) return status;
	else if (port == 1) return portd;

	return 0xff;
}

void msdx::hexdump(int offset, int count) {
	for (int i = 0; i < count; ++i) {
		fprintf(stderr, "%02X ", ioBuffer[offset + i]);
		if ((i & 15) == 15) fprintf(stderr, "\r\n");
	}
	if ((count & 15) != 0) fprintf(stderr, "\r\n");
}

void msdx::writeIO(word port, byte value, EmuTime::param time)
{
	port &= 0x03;
	mode = 1;

	if (port == 0) {
		status = 4;
		int error = 0;

		switch(value) {
			case 10:
				std::cerr << "Buffer ptr reset" << std::endl;
				bp = 0;
				break;

			case 11:
				std::cerr << "Buffer flush" << std::endl;
				bp = 0;
				ioBuffer[0] = 0;
				break;

			case 50:
			{
				if (userFile != NULL) {
					std::cerr << "Closing disk image file" << std::endl;
					fclose(userFile);
				}

				if (ioBuffer[0] == 0) {
					strcpy((char*)ioBuffer, "DEFAULT.DSK");
				}

				std::string p = "/users/charlie/Desktop/";
				p.append((const char*)ioBuffer);
				userFile = fopen(p.c_str(), "rb");

				if (userFile == NULL) {
					userFile = fopen("DEFAULT.DSK", "rb");
				}

				std::string openedOK;
				openedOK = userFile == NULL ? "No" : "Yes";
				std::cerr << "userFile: '" << p << "', opened ok: " << openedOK << std::endl;
			}
			break;

			case 51:
				logicalSector = ioBuffer[2] + 256 * ioBuffer[3];
				std::cerr << "set logsect: " << logicalSector << std::endl;
				if (userFile) fseek(userFile, logicalSector * 512, SEEK_SET);
				break;

			case 52:
				//fprintf(stderr, "read sector: %d\r\n", logicalSector);
				if (userFile != NULL) {
					fread(ioBuffer,512,1,userFile);
					++logicalSector;
					mode = 0;
					bp = 0;
				}
				error = (userFile == NULL) ? 6 : 0;
				break;

			case 53:
				//fprintf(stderr, "write sector: %d\r\n", logicalSector);
				if (userFile != NULL) {
					fwrite(ioBuffer,512,1,userFile);
					++logicalSector;
				}
				error = (userFile == NULL) ? 6 : 0;
				break;
		}
		iodata = error;
		status = 0;
	} else if (port == 1) {
		ioBuffer[bp] = value;
		bp = (bp + 1) & 511;
	}
}


template<typename Archive>
void msdx::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("dataReg", dataReg);
	ar.serialize("controlReg", controlReg);
}
INSTANTIATE_SERIALIZE_METHODS(msdx);
REGISTER_MSXDEVICE(msdx, "msdx");

} // namespace openmsx
