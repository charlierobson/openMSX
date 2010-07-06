// $Id$

#include "LocalFileReference.hh"
#include "File.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "StringOp.hh"
#include <cstdio>
#include <cstdlib>
#include <cassert>

using std::string;

namespace openmsx {

LocalFileReference::LocalFileReference(const Filename& filename)
{
	init(filename.getResolved());
}

LocalFileReference::LocalFileReference(const string& url)
{
	init(url);
}

void LocalFileReference::init(const string& url)
{
	File file(url);
	tmpFile = file.getLocalReference();
	if (!tmpFile.empty()) {
		// file is backed on the (local) filesystem,
		// we can simply use the path to that file
		assert(tmpDir.empty()); // no need to delete file/dir later
		return;
	}

	// create temp dir
#ifdef _WIN32
	tmpDir = FileOperations::getTempDir() + FileOperations::nativePathSeparator + "openmsx";
#else
	// TODO - why not just use getTempDir()?
	tmpDir = StringOp::Builder() << "/tmp/openmsx." << int(getpid());
#endif
	// it's possible this directory already exists, in that case the
	// following function does nothing
	FileOperations::mkdirp(tmpDir);

	// create temp file
	FILE* fp = FileOperations::openUniqueFile(tmpDir, tmpFile);
	if (!fp) {
		throw FileException("Couldn't create temp file");
	}

	// write temp file
	const byte* buf = file.mmap();
	unsigned size = file.getSize();
	if (fwrite(buf, 1, size, fp) != size) {
		throw FileException("Couldn't write temp file");
	}
	fclose(fp);
}

LocalFileReference::~LocalFileReference()
{
	if (!tmpDir.empty()) {
		FileOperations::unlink(tmpFile);
		// it's possible the directory is not empty, in that case
		// the following function will fail, we ignore that error
		FileOperations::rmdir(tmpDir);
	}
}

const string LocalFileReference::getFilename() const
{
	assert(!tmpFile.empty());
	return tmpFile;
}

} // namespace openmsx
