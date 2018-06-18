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

	try {
		memset((void*)ioBuffer, 0, 512);
		FILE* driveafile = fopen(userDataFileContext("msdx-sdcard/.MSDX").resolve("drive-a.txt").c_str(), "rb");
		if (driveafile) {
			fread(ioBuffer, 512, 1, driveafile);
			imgs[0] = fopen(userDataFileContext("msdx-sdcard/.MSDX").resolve((char*)ioBuffer).c_str(), "rb");
			fclose(driveafile);
		}
	}
	catch(...) {}

	imgs[1] = NULL;
	changed[0] = FALSE;
	changed[1] = FALSE;
	std::cerr << "MSDX RESET" << std::endl;
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
			case 3:
				std::cerr << "MSDX V0.01 2018 SirMorris" << std::endl;
				strcpy((char*)ioBuffer, "MSDX V0.01 2018 SirMorris");
				mode = 0;
				bp = 0;
				break;

			case 10:
				std::cerr << "Buffer ptr reset" << std::endl;
				bp = 0;
				break;

			case 11:
				std::cerr << "Buffer flush" << std::endl;
				ioBuffer[0] = 0;
				bp = 0;
				break;

			case 50: {
				// behaviour:
				// if no name is specified (ioBuffer[0] is 0) then default.dsk is the file to try to open
				// if the first open attempt does not work then add '.dsk. extension and retry
				// return error if no image was opened
				// only when image is open successfully should we overwrite FIL struct and set changed flag

				int drive = 0;

				if (ioBuffer[0] == 0) {
					strcpy((char*)ioBuffer, "DEFAULT.DSK");
				}

				//auto paths = userDataFileContext("msdx-sdcard").getPaths();
				//for(std::vector<string>::iterator it = paths.begin(); it != paths.end(); ++it) {
				//	std::cerr << *it << std::endl;
				//}

				FILE* newImg;
				string fn((char*)ioBuffer);

				try {
					newImg = fopen(userDataFileContext("msdx-sdcard").resolve(fn).c_str(), "rb");
				}
				catch(...) {
				}

				if (!newImg) {
					fn += ".dsk";
					try {
						newImg = fopen(userDataFileContext("msdx-sdcard").resolve(fn).c_str(), "rb");
					}
					catch (...) {
					}
				}

				if (newImg) {
					if (imgs[drive]) {
						fclose(imgs[drive]);
					}
					imgs[drive] = newImg;
					std::cerr << "drive: " << drive << " image file: '" << fn << "', opened ok." << std::endl;
					changed[drive] = 1;

					try {
						FILE* driveafile = fopen(userDataFileContext("msdx-sdcard/.MSDX").resolve("drive-a.txt").c_str(), "wb");
						if (driveafile) {
							fwrite(ioBuffer, strlen((char*)ioBuffer) + 1, 1, driveafile);
							fclose(driveafile);
						}
					} catch(...) {}
				}
				else {
					std::cerr << "failed to open image file: '" << fn << "', drive " << drive << " unchanged." << std::endl;
				}

				error = newImg ? 0 : 0x86;
			}
			break;

			case 51: {
				auto nSectors = int(ioBuffer[5]);
				currentDrive = int(ioBuffer[1]);
				logicalSector = ioBuffer[2] + 256 * ioBuffer[3];
				std::cerr << "drive: " << currentDrive << " logical sector: " << logicalSector << " nSectors: " << nSectors << std::endl;

				if (imgs[currentDrive]) {
					fseek(imgs[currentDrive], logicalSector * 512, SEEK_SET);
				}
				else {
					error = 0x86;
				}
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

			case 129: {
				int drive = int(ioBuffer[0]);
				std::cerr << "Drive("<< drive <<") changed?" << changed[drive] << std::endl;
				ioBuffer[0] = 0; // unknown

				if (imgs[currentDrive]) {
					ioBuffer[0] = changed[drive] ? 0xff : 0x01;
					changed[drive] = 0;
				}
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
