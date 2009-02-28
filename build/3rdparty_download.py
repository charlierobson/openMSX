# $Id$

from download import downloadURL

from os.path import isdir, isfile, join as joinpath
from urlparse import urljoin
import sys

#TODO:
#	Specify downloads for:
#	- MINGW (DX7)
#	- VC++
#	- OTHER
#
# required components -> required packages -> minus system packages
#   -> 3rdparty packages

#Create Generic download class
class Package(object):
	downloadURL = None
	name = None
	version = None

	@classmethod
	def getTarballName(cls):
		return '%s-%s.tar.gz' % (cls.name, cls.version)

	@classmethod
	def getURL(cls):
		return urljoin(cls.downloadURL + '/', cls.getTarballName())

#Download Packages
class GLEW(Package):
	downloadURL = 'http://downloads.sourceforge.net/glew'
	name = 'glew'
	version = '1.5.1'

	@classmethod
	def getTarballName(cls):
		return '%s-%s-src.tgz' % (cls.name, cls.version)

class ZLib(Package):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	name = 'zlib'
	version = '1.2.3'

class LibPNG(Package):
	downloadURL = 'http://downloads.sourceforge.net/libpng'
	name = 'libpng'
	version = '1.2.34'

class TCL(Package):
	downloadURL = 'http://downloads.sourceforge.net/tcl'
	name = 'tcl'
	version = '8.5.6'

	@classmethod
	def getTarballName(cls):
		return '%s%s-src.tar.gz' % (cls.name, cls.version)

class SDL(Package):
	downloadURL = 'http://www.libsdl.org/release'
	name = 'SDL'
	version = '1.2.13'

class SDL_image(Package):
	downloadURL = 'http://www.libsdl.org/projects/SDL_image/release'
	name = 'SDL_image'
	version = '1.2.7'

class SDL_ttf(Package):
	downloadURL = 'http://www.libsdl.org/projects/SDL_ttf/release'
	name = 'SDL_ttf'
	version = '2.0.9'

class Freetype(Package):
	downloadURL = 'http://nongnu.askapache.com/freetype'
	name = 'freetype'
	version = '2.3.7'

class LibXML2(Package):
	downloadURL = 'http://xmlsoft.org/sources'
	name = 'libxml2'
	version = '2.7.2'

class DIRECTX(Package):
	downloadURL = 'http://alleg.sourceforge.net/files'
	name = 'dx'
	version = '70'

	@classmethod
	def getTarballName(cls):
		return '%s%s_mgw.tar.gz' % (cls.name, cls.version)

def downloadPackages(packages):
	for package in packages:
		if isfile(joinpath(tarballsDir, package.getTarballName())):
			print '%s version %s - already downloaded' % (
				package.name, package.version
				)
		else:
			downloadURL(package.getURL(), tarballsDir)

if __name__ == '__main__':
	if len(sys.argv) == 3:
		platform = sys.argv[1] # One of { Win32, x64, mingw }
		tarballsDir = sys.argv[2]

		#Make Package selection
		if platform == 'Win32':
			download_packages = (
				GLEW, ZLib, LibPNG, TCL, SDL, SDL_image, SDL_ttf, Freetype,
				LibXML2
				)
		elif platform == 'x64':
			download_packages = (
				GLEW, ZLib, LibPNG, TCL, SDL, SDL_image, SDL_ttf, Freetype,
				LibXML2
				)
		elif platform == 'mingw':
			download_packages = (
				GLEW, ZLib, LibPNG, TCL, SDL, SDL_image, SDL_ttf, Freetype,
				LibXML2, DIRECTX
				)
			#add platform etc
		else:
			print >> sys.stderr, 'Unknown platform "%s"' % platform
			sys.exit(2)

		if not isdir(tarballsDir):
			print >> sys.stderr, \
				'Output directory "%s" does not exist' % tarballsDir
			sys.exit(2)

		downloadPackages(download_packages)
	else:
		print >> sys.stderr, \
			'Usage: python 3rdparty_download.py platform TARBALLS_DIR'
		sys.exit(2)
