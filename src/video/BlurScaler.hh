// $Id$

#ifndef _BLURSCALER_HH_
#define _BLURSCALER_HH_

#include "Scaler.hh"
#include "Blender.hh"

namespace openmsx {

class IntegerSetting;



template<typename Pixel> class Multiply;

template<> class Multiply<word> {
public:
	Multiply(SDL_PixelFormat* format);
	inline word multiply(word p, unsigned factor);
	void setFactor(unsigned factor);
	inline unsigned mul32(word p);
	inline word conv32(unsigned p);
private:
	unsigned tab[0x10000];
	unsigned factor;
	unsigned Rshift1, Gshift1, Bshift1;
	unsigned Rshift2, Gshift2, Bshift2;
	unsigned Rshift3, Gshift3, Bshift3;
	word     Rmask1,  Gmask1,  Bmask1; 
	word     Rmask2,  Gmask2,  Bmask2; 
};

template<> class Multiply<unsigned> {
public:
	Multiply(SDL_PixelFormat* format);
	inline unsigned multiply(unsigned p, unsigned factor);
	inline void setFactor(unsigned factor);
	inline unsigned mul32(unsigned p);
	inline unsigned conv32(unsigned p);
private:
	unsigned factor;
};


template <class Pixel>
class BlurScaler : public Scaler<Pixel>
{
public:
	BlurScaler(SDL_PixelFormat* format);
	virtual ~BlurScaler();

	virtual void scaleBlank(Pixel colour, SDL_Surface* dst,
	                        int dstY, int endDstY);
	virtual void scale256(SDL_Surface* src, int srcY, int endSrcY,
	                      SDL_Surface* dst, int dstY);
	virtual void scale512(SDL_Surface* src, int srcY, int endSrcY,
	                      SDL_Surface* dst, int dstY);

private:
	void blur256(const Pixel* pIn, Pixel* pOut, unsigned alpha);
	void blur512(const Pixel* pIn, Pixel* pOut, unsigned alpha);
	void average(const Pixel* src1, const Pixel* src2, Pixel* dst,
	             unsigned alpha);
	
	IntegerSetting& scanlineSetting;
	IntegerSetting& blurSetting;
	Blender<Pixel> blender;
	Multiply<Pixel> mult1;
	Multiply<Pixel> mult2;
	Multiply<Pixel> mult3;
};

} // namespace openmsx

#endif // _BLURSCALER_HH_


