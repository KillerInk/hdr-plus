#include "Halide.h"
#include "halide_load_raw.h"
#include "USEMAIN.h"


#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../include/stb_image_write.h"

#include "align.h"
#include "merge.h"
#include "finish.h"
#include "libraw.h"
#include "DngProfile.h"
#include "CustomMatrix.h"
#include "DngWriter.h"
#include <conio.h>
#include "dngstack.h"

using namespace Halide;

/*
 * HDRPlus Class -- Houses file I/O, defines pipeline attributes and calls
 * processes main stages of the pipeline.
 */
class HDRPlus {

    private:

        const Buffer<uint16_t> imgs;
	

    public:

        // dimensions of pixel phone output images are 3036 x 4048

        int width;
        int height;
		

        const BlackPoint bp;
        const WhitePoint wp;
        const WhiteBalance wb;
        const Compression c;
        const Gain g;

        HDRPlus(Buffer<uint16_t> imgs, BlackPoint bp, WhitePoint wp, WhiteBalance wb, Compression c, Gain g, int width1, int height1) : imgs(imgs), bp(bp), wp(wp), wb(wb), c(c), g(g) {

            width = width1;
            height = height1;
            assert(imgs.dimensions() == 3);         // width * height * img_idx
            assert(imgs.width() == width);
            assert(imgs.height() == height);
            //assert(imgs.extent(2) >= 2);            // must have at least one alternate image
        }

        /*
         * process -- Calls all of the main stages (align, merge, finish) of the pipeline.
         */

		Buffer<uint16_t> processmerge() {
			
			cout << "Evaluating alignment\n"<<endl;
			Func alignment = align(imgs);
			cout<< "Evaluating merge\n" << endl;
			Func merged = merge(imgs, alignment);
			
			Buffer<uint16_t> output_img1(width, height);
			
			merged.realize(output_img1);
			return output_img1;
		}

        Buffer<uint8_t> process() {
		    cout<<"Evaluating alignment\n"<<endl;
            Func alignment = align(imgs);
			//alignment.trace_loads();
			cout<<"Evaluating merge\n"<< endl;
            Func merged = merge(imgs, alignment);
			
            Func finished = finish(merged, width, height, bp, wp, wb, c, g);
			

            ///////////////////////////////////////////////////////////////////////////
            // realize image
            ///////////////////////////////////////////////////////////////////////////
			Buffer<uint8_t> output_img(3,width, height);
			cout << "Evaluating finish realize\n"<<endl;
            finished.realize(output_img);
			cout << "Evaluating finish realize done\n"<< endl;
            // transpose to account for interleaved layout

            output_img.transpose(0, 1);
            output_img.transpose(1, 2);
			cout << "return output\n"<<endl;
			return output_img;
        }

        /*
         * load_raws -- Loads CR2 (Canon Raw) files into a Halide Image.
         */
		static bool load_raws(std::vector<std::string> &img_names, Buffer<uint16_t> &imgs, int width1, int height1, DngProfile *profile, CustomMatrix *matrix);

        /*
         * save_png -- Writes an interleaved Halide image to an output file.
         */
        static bool save_png(std::string dir_path, std::string img_name, Buffer<uint8_t> &img) {

            std::string img_path = dir_path + "/" + img_name;

            std::remove(img_path.c_str());

            int stride_in_bytes = img.width() * img.channels();

            if(!stbi_write_png(img_path.c_str(), img.width(), img.height(), img.channels(), img.data(), stride_in_bytes)) {

                std::cerr << "Unable to write output image '" << img_name << "'" << std::endl;
                return false;
            }

            return true;
        }
};

/*
 * read_white_balance -- Reads white balance multipliers from file and returns WhiteBalance.
 */
const WhiteBalance read_white_balance(std::string file_path) {

	Tools::Internal::PipeOpener f(("../tools/dcraw -v -i " + file_path).c_str(), "r");

	char buf[1024];

	while (f.f != nullptr) {

		f.readLine(buf, 1024);

		float r, g0, g1, b;

		if (sscanf(buf, "Camera multipliers: %f %f %f %f", &r, &g0, &b, &g1) == 4) {

			float m = std::min(std::min(r, g0), std::min(g1, b));

			return { r / m, g0 / m, g1 / m, b / m };
		}
	}

	return { 1, 1, 1, 1 };
}

#ifndef USE_MAIN

int main(int argc, char* argv[]) {
	for (size_t i = 0; i < argc; i++)
	{
		cout << argv[i] << endl;
	}


    Compression c = 3.8f;
    Gain g = 1.1f;
    int w;
    int h;
    int black =0;
    int white = 0;

    int i = 1;

    while(argv[i][0] == '-') {

        if(argv[i][1] == 'c') {

            c = atof(argv[++i]);
            i++;
            continue;
        }
        else if(argv[i][1] == 'g') {

            g = atof(argv[++i]);
            i++;
            continue;
        }
        else if(argv[i][1] == 'b') {

            black = atof(argv[++i]);
            i++;
            continue;
        }
        else if(argv[i][1] == 'w' && argv[i][2] == 'p') {

            white = atof(argv[++i]);
            i++;
            continue;
        }
        else {
            std::cerr << "Invalid flag '" << argv[i][1] << "'" << std::endl;
            return 1;
        }
    }


    std::vector<std::string> in_names;

    while (i < argc) in_names.push_back(argv[i++]);

    Buffer<uint16_t> imgs;
	DngProfile * dngProfile = new DngProfile();
	CustomMatrix* matrix = new CustomMatrix();
	w = 5312;
	h = 2988;

	Tools::getSize(in_names[0], w, h);

	cout << "getsize W:" << w << " H:" << h << "\n" <<endl;
		

    if(!HDRPlus::load_raws(in_names, imgs, w,h, dngProfile, matrix)) return -1;

    const WhiteBalance wb = read_white_balance(in_names[0]);
	cout << "wb:" << wb.b << wb.g0 << wb.g1 << wb.r << endl;
    BlackPoint bp = 64;
    WhitePoint wp = 1023;
    if(black != 0)
        bp = black;
    if(white != 0)
        wp = white;

    HDRPlus hdr_plus = HDRPlus(imgs, bp, wp, wb, c, g, w,h);
	
	Buffer<uint16_t> outdata = hdr_plus.processmerge();
	Tools::write_dng(w, h, outdata.data(), (in_names[0]+"_merged.dng").c_str(),dngProfile, matrix);

    return 0;
}
#endif // !USE_MAIN

/*
* load_raws -- Loads CR2 (Canon Raw) files into a Halide Image.
*/

inline bool HDRPlus::load_raws(std::vector<std::string>& img_names, Buffer<uint16_t>& imgs, int width1, int height1, DngProfile *dngprofile, CustomMatrix* matrix) {

	int num_imgs = img_names.size();
	cout << " load raws \n "<<endl;
	imgs = Buffer<uint16_t>(width1, height1, num_imgs);
	cout << "buffer created \n "<<endl;
	uint16_t *data = imgs.data();

	for (int n = 0; n < num_imgs; n++) {

		std::string img_name = img_names[n];
		cout << " load raw" << img_name <<" \n"<< endl;
		if (!Tools::load_image(img_name, data, width1, height1,dngprofile,matrix, (n==0))) {

			std::cerr << "Input image failed to load" << std::endl;

			cout << " Input image failed to load" << img_name << " \n" << endl;
			return false;
		}

		data += width1 * height1;
	}
	return true;
}
