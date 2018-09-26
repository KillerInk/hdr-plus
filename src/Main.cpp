
#include "USEMAIN.h"

#ifdef USE_MAIN
#include "Halide.h"
#include "align.h"
#include "merge.h"
#include "finish.h"
using namespace Halide;

Func alignmerge(ImageParam imgs, ImageParam imgs2)
{
	Func alignment = align(imgs);
	return merge(imgs2, alignment);
}

int main(int argc, char* argv[]) {
	
	ImageParam imgs(type_of<uint16_t>(), 3);
	ImageParam alignImgs(type_of<uint16_t>(), 3);
	//Func alignbuf;
	//ImageParam alignbuf(type_of<uint16_t>(), 3);

	Target target;
	target.os = Target::Android; // The operating system
	target.arch = Target::ARM;   // The CPU architecture
	target.bits = 32;            // The bit-width of the architecture
	std::vector<Target::Feature> arm_features; // A list of features to set
	//arm_features.push_back(Target::LargeBuffers);
	//arm_features.push_back(Target::NoNEON);
	target.set_features(arm_features);

	//Func alignment = align(imgs);
	Func stage1_alignmerge = alignmerge(alignImgs, imgs);

	stage1_alignmerge.compile_to_static_library("stage1_alignmerge", {  alignImgs,imgs }, "stage1_alignmerge", target);
	
	ImageParam input(type_of<uint16_t>(), 2);
	Param<int> blackpoint;
	Param<int> whitepoint;
	Param<float> wb_r;
	Param<float> wb_g1;
	Param<float> wb_g2;
	Param<float> wb_b;
	Param<float> compression;
	Param<float> gain;

	Func stage2_RawToRgb = finish(input, blackpoint, whitepoint, wb_r,wb_g1,wb_g2,wb_b, compression, gain);
	std::vector<Argument> args(9);
	args[0] = input;
	args[1] = blackpoint;
	args[2] = whitepoint;
	args[3] = wb_r;
	args[4] = wb_g1;
	args[5] = wb_g2;
	args[6] = wb_b;
	args[7] = compression;
	args[8] = gain;

	//stage2_RawToRgb.compile_to_static_library("stage2_RawToRgb", { args }, "stage2_RawToRgb", target);

	//merged.realize(output_img1);
	return 0;
}
#endif // USE_MAIN

