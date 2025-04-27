#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Error: Expected at least two arguments: a path to a reference image and one"
                  << " or more paths to images to compare it to." << std::endl;
        return 1;
    }

    int ref_width, ref_height, ref_comp;
    float* reference = stbi_loadf(argv[1], &ref_width, &ref_height, &ref_comp, 4);
    std::cout << "Reference: " << argv[1] << std::endl;

    for (int image_idx = 2; image_idx < argc; image_idx++) {

        int width, height, comp;
        float* image = stbi_loadf(argv[image_idx], &width, &height, &comp, 4);

        if (image == NULL) {
            std::cerr << "Error: Could not read " << argv[image_idx] << ": " << stbi_failure_reason() << std::endl;
            continue;
        }

        if (width != ref_width || height != ref_height || comp != ref_comp) {
            std::cerr << "Error: Image at " << argv[image_idx] << " has dimensions and channel number incompatible"
                      << " with the reference." << std::endl;
            continue;
        }

        double mean = 0, sse = 0;

        for (size_t i = 0; i < width * height * 4; i++) {
            if ((i % 4) == 3)
                continue;
            double error = image[i] - reference[i];
            sse += error * error;
        }

        double mse = sse / (width * height * 3 - 1);

        std::cout << argv[image_idx] << "\tMSE: " << mse << std::endl;

        stbi_image_free(image);
    }

    stbi_image_free(reference);
}