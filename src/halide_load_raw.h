// basic raw image loader for Halide::Buffer<T> or other image with same API
// code largely appropriated from halide_image_io.h

#ifndef HALIDE_LOAD_RAW_H
#define HALIDE_LOAD_RAW_H

#include <string>
using namespace std;

#include "Halide.h"
#include "halide_image_io.h"
#include "libraw.h"
#include "DngProfile.h"
#include "CustomMatrix.h"
#include "DngWriter.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"

namespace Halide {
namespace Tools {

namespace Internal {

struct PipeOpener {
    PipeOpener(const char* cmd, const char* mode) : f(fopen(cmd, mode)) {
        // nothing
    }
    ~PipeOpener() {
        if (f != nullptr) {
            fclose(f);
        }
    }
	// read a line of data skipping lines that begin with '#"
	char *readLine(char *buf, int maxlen) {
		char *status;
		do {
			status = fgets(buf, maxlen, f);
		} while (status && buf[0] == '#');
		return(status);
	}
    FILE * const f;
};



} // namespace Internal

inline bool is_little_endian() {
    int value = 1;
    return ((char *) &value)[0] == 1;
}

inline void swap_endian_16(uint16_t &value) {
    value = ((value & 0xff)<<8)|((value & 0xff00)>>8);
}


void copyMatrix(float* dest, float colormatrix[4][3])
{
	int m = 0;
	for (int i = 0; i < 3; i++)
	{
		for (int t = 0; t < 3; t++)
		{
			dest[m++] = colormatrix[i][t];
		}
	}
}
void copyMatrix(float* dest, float colormatrix[3][4])
{
	int m = 0;
	for (int i = 0; i < 3; i++)
	{
		for (int t = 0; t < 3; t++)
		{
			dest[m++] = colormatrix[i][t];
		}
	}
}

void copyMatrix(float* dest, float colormatrix[4])
{
	int m = 0;
	for (int i = 0; i < 3; i++)
	{
		dest[m++] = colormatrix[i];
	}
}

template<Internal::CheckFunc check = Internal::CheckFail>
bool load_raw(const std::string &filename, uint16_t* data, int width, int height, DngProfile *dngprofile,CustomMatrix* matrix, bool readmeta) {
	
	LibRaw raw;
	int ret;
	raw.imgdata.params.no_auto_bright = 0; //-W
	raw.imgdata.params.use_camera_wb = 1;
	raw.imgdata.params.output_bps = 16; // -6
	raw.imgdata.params.output_color = 0;
	//raw.imgdata.params.user_qual = 0;
    //raw.imgdata.params.half_size = 1;
	raw.imgdata.params.no_auto_scale = 0;
	raw.imgdata.params.gamm[0] = 1.0; //-g 1 1
	raw.imgdata.params.gamm[1] = 1.0; //-g 1 1
	raw.imgdata.params.output_tiff = 0;
	raw.imgdata.params.no_interpolation = 1;
	if ((ret = raw.open_file(filename.c_str())) != LIBRAW_SUCCESS)
		return false;
	cout << "open file:" << filename << "\n";
	if ((ret = raw.unpack()) != LIBRAW_SUCCESS)
		return false;
	cout << "unpack file:" << filename << "\n";

	if (readmeta)
	{
		cout << "create dng profile start:" << filename << "\n";
		float* bl = new float[4];
		for (size_t i = 0; i < 4; i++)
		{
			bl[i] = raw.imgdata.color.dng_levels.dng_cblack[6];
		}
		dngprofile->blacklevel = bl;
		dngprofile->whitelevel = raw.imgdata.color.dng_levels.dng_whitelevel[0];
		dngprofile->rawwidht = width;
		dngprofile->rawheight = height;
		dngprofile->rowSize = 0;

		char cfaar[4];
		cfaar[0] = raw.imgdata.idata.cdesc[raw.COLOR(0, 0)];
		cfaar[1] = raw.imgdata.idata.cdesc[raw.COLOR(0, 1)];
		cfaar[2] = raw.imgdata.idata.cdesc[raw.COLOR(1, 0)];
		cfaar[3] = raw.imgdata.idata.cdesc[raw.COLOR(1, 1)];

		string cfa = cfaar;
		if (cfa == "BGGR")
		{
			dngprofile->bayerformat = "bggr";
		}
		else if (cfa == "RGGB")
		{
			dngprofile->bayerformat = "rggb";
		}
		else if (cfa == "GRBG")
		{
			dngprofile->bayerformat = "grbg";
		}
		else
		{
			dngprofile->bayerformat = "gbrg";
		}
	
		dngprofile->rawType = 6;

		cout << "create dng profile end:" << filename << "\n";

		cout << "copy colormatrix start:" << filename << "\n";
		matrix->colorMatrix1 = new float[9];
		matrix->colorMatrix2 = new float[9];
		matrix->neutralColorMatrix = new float[3];
		matrix->fowardMatrix1 = new float[9];
		matrix->fowardMatrix2 = new float[9];
		matrix->reductionMatrix1 = new float[9];
		matrix->reductionMatrix2 = new float[9];

		copyMatrix(matrix->colorMatrix1, raw.imgdata.color.dng_color[0].colormatrix);
		copyMatrix(matrix->colorMatrix2, raw.imgdata.color.dng_color[1].colormatrix);
		copyMatrix(matrix->neutralColorMatrix, raw.imgdata.color.cam_mul);
		copyMatrix(matrix->fowardMatrix1, raw.imgdata.color.dng_color[0].forwardmatrix);
		copyMatrix(matrix->fowardMatrix2, raw.imgdata.color.dng_color[1].forwardmatrix);
		copyMatrix(matrix->reductionMatrix1, raw.imgdata.color.dng_color[0].calibration);
		copyMatrix(matrix->reductionMatrix2, raw.imgdata.color.dng_color[1].calibration);
		cout << "copy colormatrix end:" << filename << "\n";
	}
	int t = 0;
	for (size_t i = 0; i <  width *  height; i++)
	{
		data[i] = raw.imgdata.rawdata.raw_image[i] << 2;
	}
	cout << "rawdatacopy end:" << filename << "\n";

	raw.recycle();
	cout << "raw recycled:" << filename << "\n";
    return true;
}

bool getSize(const std::string &filename, int &width, int &height)
{
	LibRaw raw;
	int ret;
	if ((ret = raw.open_file(filename.c_str())) != LIBRAW_SUCCESS)
		return false;
	if ((ret = raw.unpack()) != LIBRAW_SUCCESS)
		return false;
	width = raw.imgdata.sizes.raw_width;
	height = raw.imgdata.sizes.raw_height;
	raw.recycle();
	return true;
}

void write_ppm(unsigned width, unsigned height, unsigned short *bitmap, const char *fname)
{
	if (!bitmap)
		return;

	FILE *f = fopen(fname, "wb");
	if (!f)
		return;
	int bits = 16;
	fprintf(f, "P5\n%d %d\n%d\n", width, height, (1 << bits) - 1);
	unsigned char *data = (unsigned char *)bitmap;
	unsigned data_size = width * height * 2;
#define SWAP(a, b)                                                                                                     \
  {                                                                                                                    \
    a ^= b;                                                                                                            \
    a ^= (b ^= a);                                                                                                     \
  }
	for (unsigned i = 0; i < data_size; i += 2)
		SWAP(data[i], data[i + 1]);
#undef SWAP

	fwrite(data, data_size, 1, f);
	fclose(f);
}
void write_dng(unsigned width, unsigned height, unsigned short *bitmap, const char *fname, DngProfile * dngprof, CustomMatrix * matrix)
{
	if (!bitmap)
		return;

	int bits = 16;
	unsigned char *data1 = (unsigned char *)bitmap;
	unsigned data_size = width * height * 2;
	
	DngWriter *dngw = new DngWriter();

	dngw->dngProfile = dngprof;
	dngw->customMatrix = matrix;
	dngw->bayerBytes = data1;
	dngw->rawSize = data_size;
	dngw->fileSavePath = (char*)fname;
	dngw->_make = "hdr+";
	dngw->_model = "model";

	dngw->WriteDNG();
}

void write_ppm(libraw_processed_image_t *img, const char *basename)
{
	if (!img)
		return;
	// type SHOULD be LIBRAW_IMAGE_BITMAP, but we'll check
	if (img->type != LIBRAW_IMAGE_BITMAP)
		return;
	// only 3-color images supported...
	if (img->colors != 3)
		return;

	char fn[1024];
	snprintf(fn, 1024, "%s.ppm", basename);
	FILE *f = fopen(fn, "wb");
	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n%d\n", img->width, img->height, (1 << img->bits) - 1);
	/*
	NOTE:
	data in img->data is not converted to network byte order.
	So, we should swap values on some architectures for dcraw compatibility
	(unfortunately, xv cannot display 16-bit PPMs with network byte order data
	*/
	#define SWAP(a, b)                                                                                                     \
	{                                                                                                                    \
	a ^= b;                                                                                                            \
	a ^= (b ^= a);                                                                                                     \
	}
		for (unsigned i = 0; i < img->data_size; i += 2)
			SWAP(img->data[i], img->data[i + 1]);
	#undef SWAP

	fwrite(img->data, img->data_size, 1, f);
	fclose(f);
}

template<Internal::CheckFunc check = Internal::CheckFail>
bool load_pgm(const std::string &filename, uint16_t* data, int width, int height)
{
	Internal::PipeOpener f((filename).c_str(), "r");
	//if (!check(f.f != nullptr, "File %s could not be opened for reading\n", filename.c_str())) return false;
	int in_width, in_height, maxval;
	char header[256];
	char buf[1024];
	bool fmt_binary = false;

	f.readLine(buf, 1024);
	if (!check(sscanf(buf, "%255s", header) == 1, "Could not read PGM header\n")) return false;
	if (header == std::string("P5") || header == std::string("p5"))
		fmt_binary = true;
	if (!check(fmt_binary, "Input is not binary PGM\n")) return false;

	f.readLine(buf, 1024);
	if (!check(sscanf(buf, "%d %d\n", &in_width, &in_height) == 2, "Could not read PGM width and height\n")) return false;

	//if (!check(in_width == width, "Input image '%s' has width %d, but must must have width of %d\n", filename.c_str(), in_width, width)) return false;

	//if (!check(in_height == height, "Input image '%s' has height %d, but must must have height of %d\n", filename.c_str(), in_height, height)) return false;

	f.readLine(buf, 1024);
	if (!check(sscanf(buf, "%d", &maxval) == 1, "Could not read PGM max value\n")) return false;

	if (!check(maxval == 65535, "Invalid bit depth (not 16 bits) in PGM\n")) return false;
	fread((void *)data, sizeof(uint16_t), width*height, f.f);
	//if (!check( == (size_t)(width*height), "Could not read PGM 16-bit data\n")) return false;

	if (is_little_endian()) {

		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {

				swap_endian_16(data[y * width + x]);
			}
		}
	}
	return true;
}

/*inline bool save_png(std::string dir_path, std::string img_name, Buffer<uint8_t> &img) {

	std::string img_path = dir_path + "/" + img_name;

	std::remove(img_path.c_str());

	int stride_in_bytes = img.width() * img.channels();

	if (!stbi_write_png(img_path.c_str(), img.width(), img.height(), img.channels(), img.data(), stride_in_bytes)) {

		std::cerr << "Unable to write output image '" << img_name << "'" << std::endl;
		return false;
	}

	return true;
}*/

bool load_image(const std::string &filename, uint16_t* data, int width, int height,DngProfile *profile,CustomMatrix *custommatrix,bool readmeta) {
	size_t lastdot = filename.find_last_of('.');
	std::string suffix = filename.substr(lastdot + 1);

	if (suffix.compare("pgm") == 0 || suffix.compare("ppm") == 0) {
		return load_pgm(filename, data, width, height);
	}
	else {
		return load_raw(filename, data, width, height,profile,custommatrix,readmeta);
		
	}
}

} // namespace Tools
} // namespace Halide

#endif