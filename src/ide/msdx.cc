#include "msdx.hh"
#include "serialize.hh"
#include "FileContext.hh"

#include <string>
#include <iostream>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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
	imgs[0]=NULL;
	imgs[1]=NULL;
	changed[0] = FALSE;
	changed[1] = FALSE;

	auto paths = userDataFileContext("msdx-sdcard").getPaths();
	home = paths[1];

//	for(std::vector<string>::iterator it = paths.begin(); it != paths.end(); ++it) {
//		std::cerr << "'" << *it << "'" << std::endl;
//	}

	std::cerr << "sdcard home dir: " << home << std::endl;

	try {
		std::cerr << "MSDX RESET - what's in A:? " << std::endl;
		memset((void*)ioBuffer, 0, 512);
		FILE* driveafile = fopen(userDataFileContext("msdx-sdcard/.MSDX").resolve("drive-a.txt").c_str(), "rb");
		if (driveafile) {
			fread(ioBuffer, 512, 1, driveafile);
			char* p = (char*)ioBuffer;
			while(*p > 31) {
				++p;
			}
			*p = 0;
			std::cerr << "  disk in drive a: '" << (char*)ioBuffer << "'" << std::endl;
			fclose(driveafile);
			imgs[0] = fopen(userDataFileContext("msdx-sdcard").resolve((char*)ioBuffer).c_str(), "rb");
		}
	}
	catch(...) {}
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
				strcpy((char*)ioBuffer, "MSDX V0.1  2018 SirMorris");
				mode = 0;
				bp = 0;
				break;

			case 10:
				bp = 0;
				break;

			case 11:
				ioBuffer[0] = 0;
				bp = 0;
				break;

			// CMD_DIR_READ_BEGIN
			case 20:
				error = dirBegin();
				break;

			// CMD_DIR_READ_NEXT
			case 21:
				error = dirHandler();
				if (!error) {
					std::cerr << "  " << (char*)ioBuffer << std::endl;
				}
				break;

			case 50: {
				// behaviour:
				// if no name is specified (ioBuffer[0] is 0) then default.dsk is the file to try to open
				// if the first open attempt does not work then add '.dsk. extension and retry
				// return error if no image was opened
				// only when image is open successfully should we overwrite FIL struct,
				//  set changed flag and write filename to drive-a.txt

				int drive = 0;

				if (ioBuffer[0] == 0) {
					strcpy((char*)ioBuffer, "DEFAULT.DSK");
				}

				//auto paths = userDataFileContext("msdx-sdcard").getPaths();
				//for(std::vector<string>::iterator it = paths.begin(); it != paths.end(); ++it) {
				//	std::cerr << *it << std::endl;
				//}

				FILE* newImg;

				try {
					newImg = fopen(userDataFileContext("msdx-sdcard").resolve((char*)ioBuffer).c_str(), "rb");
				}
				catch(...) {
				}

				if (!newImg) {
					strcat((char*)ioBuffer, ".dsk");
					try {
						newImg = fopen(userDataFileContext("msdx-sdcard").resolve((char*)ioBuffer).c_str(), "rb");
					}
					catch (...) {
					}
				}

				if (newImg) {
					if (imgs[drive]) {
						fclose(imgs[drive]);
					}
					imgs[drive] = newImg;
					std::cerr << "drive: " << drive << " image file: '" << (char*)ioBuffer << "', opened ok." << std::endl;
					changed[drive] = 1;

					try {
						FILE* driveafile = fopen(userDataFileContext("msdx-sdcard/.MSDX").resolve("drive-a.txt").c_str(), "wb");
						if (driveafile) {
							fwrite(ioBuffer, strlen((char*)ioBuffer), 1, driveafile);
							fclose(driveafile);
						}
					} catch(...) {}
				}
				else {
					std::cerr << "failed to open image file: '" << (char*)ioBuffer << "', drive " << drive << " unchanged." << std::endl;
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
					error = 0x82;
				}
			}
			break;

			case 52:
				if (imgs[currentDrive]) {
					fread(ioBuffer, 512, 1, imgs[currentDrive]);
					++logicalSector;
					mode = 0;
					bp = 0;
				} else {
					error = 0x82;
				}
				break;

			case 53:
				if (imgs[currentDrive]) {
					fwrite(ioBuffer, 512, 1, imgs[currentDrive]);
					++logicalSector;
				} else {
					error = 0x082;
				}
				break;

			case 129: {
				int drive = int(ioBuffer[0]);
				std::cerr << "drive "<< drive <<" changed?" << changed[drive] << std::endl;
				ioBuffer[0] = 0; // unknown

				if (imgs[currentDrive]) {
					ioBuffer[0] = changed[drive] ? 0xff : 0x01;
					changed[drive] = 0;
				}
			}
			break;

			case 130: {
				std::cerr << "Ejecting drive 0" << std::endl;
				if (imgs[0]) {
					fclose(imgs[0]);
					imgs[0] = NULL;
					changed[0] = TRUE;
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


void msdx::strupper(char* dest)
{
	while(*dest)
	{
		*dest = toupper(*dest);
		++dest;
	}
}

void msdx::strvalidate(char* p)
{
	char* p2 = p;
	while(*p2)
	{
		unsigned char q = *p2 - 32;
		if (q > 96)
		{
			*p2 = '?';
		}
		++p2;
	}
	strupper(p);
}

int msdx::dirHandler()
{
	int rtn = 0;
	int switchState;

	do	
	{
		switchState = FALSE;

		switch(dirState)
		{
		case 0:
			{
				// new search
				dir = opendir(home.c_str());
				dirState = 1;
			}
			break;

		case 1:
			{
				// reading directories
				dirent* dire = readdir(dir);

				if (!dire)
				{
					// done dirs
					switchState = TRUE;
					dirState = 2;
					break;
				}
				else
				{
					if (dire->d_name[0] == '.') {
						switchState = TRUE;
						break;
					}

					struct stat path_stat;
					std::string fullpath = home;
					fullpath += "/";
					fullpath += dire->d_name;

					stat(fullpath.c_str(), &path_stat);
					if (!S_ISREG(path_stat.st_mode))
					{
						ioBuffer[0] = '<';
						strcpy((char*)ioBuffer+1, dire->d_name);
						strcat((char*)ioBuffer, ">           ");
						ioBuffer[12] = 0;
						strupper((char*)ioBuffer);
						mode = 0;
						bp = 0;
					}
					else
					{
						// go around one more time
						switchState = TRUE;
					}
				}
			}
			break;

		case 2:
			{
				// new search
				dir = opendir(home.c_str());
				switchState = TRUE;
				dirState = 3;
			}
			break;	

		case 3:
			{
				// reading files
				dirent* dire = readdir(dir);

				if (!dire)
				{
					// done files
					return 0x40;
				}
				else
				{
					if (dire->d_name[0] == '.') {
						switchState = TRUE;
						break;
					}

					struct stat path_stat;
					std::string fullpath = home;
					fullpath += "/";
					fullpath += dire->d_name;

					stat(fullpath.c_str(), &path_stat);
					if (S_ISREG(path_stat.st_mode))
					{
						strcpy((char*)ioBuffer, dire->d_name);
						strupper((char*)ioBuffer);
						mode = 0;
						bp = 0;
					}
					else
					{
						// go around one more time
						switchState = TRUE;
					}
				}
			}
			break;
		}
	}
	while (switchState);

	return rtn;
}	

int msdx::dirBegin()
{
	dirState = 0;
	return dirHandler();
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
