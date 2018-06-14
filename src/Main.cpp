
#include "USEMAIN.h"

#ifdef USE_MAIN
#include "Halide.h"
#include "align.h"
#include "merge.h"
using namespace Halide;
int main(int argc, char* argv[]) {
	
	ImageParam imgs(type_of<uint16_t>(), 3);

	Target target;
	target.os = Target::Android; // The operating system
	target.arch = Target::ARM;   // The CPU architecture
	target.bits = 32;            // The bit-width of the architecture
	std::vector<Target::Feature> arm_features; // A list of features to set
	target.set_features(arm_features);

	Func alignment = align(imgs);
	Func merged = merge(imgs, alignment);
	// We then pass the target as the last argument to compile_to_file.
	merged.compile_to_static_library("dngstack", { imgs }, "dngstack", target);

	//merged.realize(output_img1);
	return 0;
}
#endif // USE_MAIN

