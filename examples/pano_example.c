/**
 * @file pano_example.c
 * @brief Example of using OpenPano FFI from C
 *
 * This example demonstrates basic panorama stitching using the C API.
 *
 * Compile:
 *   gcc -o pano_example pano_example.c -I../ffi/include -L../dist/macos/lib -lopenpano -lc++ -framework Accelerate
 *
 * Usage:
 *   ./pano_example image1.jpg image2.jpg [image3.jpg ...] output.jpg
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "openpano_ffi.h"

/**
 * @brief Progress callback function
 *
 * Called during stitching to report progress.
 *
 * @param progress Current progress from 0.0 to 1.0
 * @param stage Description of current stage
 * @param user_data User-provided context
 */
void progress_callback(float progress, const char* stage, void* user_data) {
    (void)user_data;
    printf("[%3.0f%%] %s\n", progress * 100.0f, stage ? stage : "");
    fflush(stdout);
}

/**
 * @brief Print usage information
 *
 * @param program_name Name of the executable
 */
void print_usage(const char* program_name) {
    printf("OpenPano FFI Example\n");
    printf("====================\n\n");
    printf("Usage: %s [options] image1 image2 [image3 ...] output\n\n", program_name);
    printf("Options:\n");
    printf("  --cylinder    Use cylinder mode (default: camera estimation)\n");
    printf("  --translation Use translation mode\n");
    printf("  --no-crop     Don't crop the result\n");
    printf("  --help        Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s photo1.jpg photo2.jpg photo3.jpg panorama.jpg\n", program_name);
    printf("  %s --cylinder img1.jpg img2.jpg output.png\n", program_name);
    printf("\n");
    printf("Supported formats: JPEG, PNG\n");
}

/**
 * @brief Main entry point
 *
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success, non-zero on error
 */
int main(int argc, char* argv[]) {
    OpenpanoError err;
    OpenpanoStitcherHandle stitcher = NULL;
    OpenpanoImageHandle result = NULL;
    OpenpanoConfig config;
    int exit_code = 1;
    int first_image_arg = 1;
    int i;
    
    // Print version
    printf("OpenPano FFI v%s\n", openpano_version());
    printf("JPEG support: %s\n", openpano_has_jpeg_support() ? "yes" : "no");
    printf("PNG support: %s\n\n", openpano_has_png_support() ? "yes" : "no");
    
    // Initialize default config
    openpano_config_default(&config);
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--cylinder") == 0) {
            config.mode = OPENPANO_MODE_CYLINDER;
            config.ordered_input = 1;
            first_image_arg = i + 1;
        } else if (strcmp(argv[i], "--translation") == 0) {
            config.mode = OPENPANO_MODE_TRANSLATION;
            config.ordered_input = 1;
            first_image_arg = i + 1;
        } else if (strcmp(argv[i], "--no-crop") == 0) {
            config.crop_result = 0;
            first_image_arg = i + 1;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            break;
        }
    }
    first_image_arg = i;
    
    // Check minimum arguments (at least 2 images + output)
    if (argc - first_image_arg < 3) {
        fprintf(stderr, "Error: Need at least 2 input images and 1 output file\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Create stitcher
    printf("Creating stitcher...\n");
    stitcher = openpano_stitcher_create(&config);
    if (stitcher == NULL) {
        fprintf(stderr, "Error: Failed to create stitcher\n");
        return 1;
    }
    
    // Add input images (all args except the last one, which is output)
    printf("Adding images...\n");
    for (i = first_image_arg; i < argc - 1; i++) {
        printf("  Adding: %s\n", argv[i]);
        err = openpano_add_image_file(stitcher, argv[i]);
        if (err != OPENPANO_OK) {
            fprintf(stderr, "Error adding %s: %s\n", argv[i], openpano_error_string(err));
            goto cleanup;
        }
    }
    
    printf("\nTotal images: %d\n\n", openpano_image_count(stitcher));
    
    // Perform stitching with progress
    printf("Stitching...\n");
    err = openpano_stitch_with_progress(stitcher, &result, progress_callback, NULL);
    if (err != OPENPANO_OK) {
        fprintf(stderr, "Stitching failed: %s\n", openpano_error_string(err));
        goto cleanup;
    }
    
    // Get result dimensions
    {
        int32_t width, height;
        openpano_image_dimensions(result, &width, &height);
        printf("\nResult size: %dx%d pixels\n", width, height);
    }
    
    // Save result
    {
        const char* output_path = argv[argc - 1];
        printf("Saving to: %s\n", output_path);
        err = openpano_image_save(result, output_path, 95);
        if (err != OPENPANO_OK) {
            fprintf(stderr, "Failed to save: %s\n", openpano_error_string(err));
            goto cleanup;
        }
    }
    
    printf("\nSuccess!\n");
    exit_code = 0;
    
cleanup:
    // Clean up resources
    if (result != NULL) {
        openpano_image_destroy(result);
    }
    if (stitcher != NULL) {
        openpano_stitcher_destroy(stitcher);
    }
    
    return exit_code;
}

