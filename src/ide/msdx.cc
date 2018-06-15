#include "msdx.hh"
#include "serialize.hh"
#include "FileContext.hh"

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

void msdx::reset(EmuTime::param time __attribute__((unused)))
{
	mode = 1;
	status = 0;
	imgs[0] = NULL;
	imgs[1] = NULL;
	changed[0] = FALSE;
	changed[1] = FALSE;
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

byte msdx::readIO(word port, EmuTime::param time __attribute__((unused)))
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

byte msdx::peekIO(word port, EmuTime::param time __attribute__((unused))) const
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

void msdx::writeIO(word port, byte value, EmuTime::param time __attribute__((unused)))
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
				int drive = 0;
				changed[drive] = 1;

				if (imgs[drive]) {
					fclose(imgs[drive]);
					imgs[drive] = NULL;
				}

				if (ioBuffer[0] == 0) {
					strcpy((char*)ioBuffer, "DEFAULT.DSK");
				}

				//auto paths = userDataFileContext("msdx-sdcard").getPaths();
				//for(std::vector<string>::iterator it = paths.begin(); it != paths.end(); ++it) {
				//	std::cerr << *it << std::endl;
				//}

				string fn((char*)ioBuffer);

				try {
					imgs[drive] = fopen(userDataFileContext("msdx-sdcard").resolve(fn).c_str(), "rb");
				}
				catch(...) {
				}

				if (!imgs[drive]) {
					fn += ".dsk";
					try {
						imgs[drive] = fopen(userDataFileContext("msdx-sdcard").resolve(fn).c_str(), "rb");
					}
					catch (...) {
						imgs[drive] = NULL;
					}
				}

				std::string openedOK = !imgs[drive] ? "No" : "Yes";
				std::cerr << "drive: " << drive << " image file: '" << fn << "', opened ok: " << openedOK << std::endl;
				error = imgs[drive] ? 0 : 0x86;
			}
			break;

			case 51:
				currentDrive = int(ioBuffer[1]);
				logicalSector = ioBuffer[2] + 256 * ioBuffer[3];
				std::cerr << "drive: " << currentDrive << " logical sector: " << logicalSector << std::endl;

				if (imgs[currentDrive]) {
					fseek(imgs[currentDrive], logicalSector * 512, SEEK_SET);
				}
				else {
					error = 0x86;
				}
				break;

			case 52:
				if (!imgs[currentDrive]) {
					error = 0x86;
				} else {
					fread(ioBuffer, 512, 1, imgs[currentDrive]);
					++logicalSector;
					mode = 0;
					bp = 0;
				}
				break;

			case 53:
				if (imgs[currentDrive]) {
					fwrite(ioBuffer, 512, 1, imgs[currentDrive]);
					++logicalSector;
				} else {
					error = 0x086;
				}
				break;

			case 129:
				int drive = int(ioBuffer[1]);
				ioBuffer[0] = 0; // unknown

				if (imgs[currentDrive]) {
					ioBuffer[0] = changed[drive] ? 0xff : 0x01;
				}
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
