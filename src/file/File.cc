// $Id$

#include "File.hh"
#include "FileBase.hh"
#include "LocalFile.hh"
#include "FileContext.hh"


File::File(const FileContext *context, const std::string &url, OpenMode mode)
{
	if (url.find("://") != std::string::npos) {
		// protocol specified, don't use SearchPath
		open(url, mode);
	} else {
		std::list<std::string> pathList;
		if ((mode == LOAD_PERSISTENT) || (mode == SAVE_PERSISTENT)) {
			pathList.push_back(context->getSavePath());
		} else {
			pathList = context->getPathList();
		}
		std::list<std::string>::const_iterator it;
		for (it = pathList.begin(); it != pathList.end(); it++) {
			try {
				open(*it + url, mode);
				return;
			} catch (FileException &e) {
				// try next
			}
		}
		throw FileException("Error opening file " + url);
	}
}

File::~File()
{
	delete file;
}


void File::open(const std::string &url, OpenMode mode)
{
	std::string protocol, name;
	unsigned int pos = url.find("://");
	if (pos == std::string::npos) {
		// no explicit protocol, take "file"
		protocol = "file";
		name = url;
	} else {
		protocol = url.substr(0, pos);
		name = url.substr(pos + 3);
	}
	
	PRT_DEBUG("File: " << protocol << "://" << name);
	if (protocol == "file") {
		file = new LocalFile(name, mode);
	} else {
		PRT_ERROR("Unsupported protocol: " << protocol);
	}
}

void File::read(byte* buffer, int num)
{
	file->read(buffer, num);
}

void File::write(const byte* buffer, int num)
{
	file->write(buffer, num);
}

byte* File::mmap(bool writeBack)
{
	return file->mmap(writeBack);
}

void File::munmap()
{
	file->munmap();
}

int File::getSize()
{
	return file->getSize();
}

void File::seek(int pos)
{
	file->seek(pos);
}

int File::getPos()
{
	return file->getPos();
}

const std::string File::getURL() const
{
	return file->getURL();
}

const std::string File::getLocalName() const
{
	return file->getLocalName();
}
