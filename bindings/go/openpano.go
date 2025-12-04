// Package openpano provides Go bindings for the OpenPano panorama stitching library.
//
// # Overview
//
// OpenPano is a high-quality panorama stitching library that supports:
//   - Automatic camera estimation and bundle adjustment
//   - Cylindrical stitching for standard panoramas
//   - Translation mode for document scanning
//   - Multi-band blending for smooth seams
//
// # Example
//
//	package main
//
//	import (
//	    "fmt"
//	    "log"
//	    "github.com/openpano/go-openpano"
//	)
//
//	func main() {
//	    config := openpano.DefaultConfig()
//	    config.Mode = openpano.ModeEstimateCamera
//	    config.CropResult = true
//
//	    stitcher, err := openpano.NewStitcher(config)
//	    if err != nil {
//	        log.Fatal(err)
//	    }
//	    defer stitcher.Close()
//
//	    if err := stitcher.AddImage("photo1.jpg"); err != nil {
//	        log.Fatal(err)
//	    }
//	    if err := stitcher.AddImage("photo2.jpg"); err != nil {
//	        log.Fatal(err)
//	    }
//
//	    result, err := stitcher.Stitch()
//	    if err != nil {
//	        log.Fatal(err)
//	    }
//	    defer result.Close()
//
//	    if err := result.Save("panorama.jpg", 90); err != nil {
//	        log.Fatal(err)
//	    }
//
//	    width, height, _ := result.Dimensions()
//	    fmt.Printf("Created %dx%d panorama\n", width, height)
//	}
package openpano

/*
#cgo CFLAGS: -I${SRCDIR}/../../ffi/include
#cgo darwin LDFLAGS: -L${SRCDIR}/../../dist/macos/lib -lopenpano -lc++ -framework Accelerate
#cgo linux LDFLAGS: -L${SRCDIR}/../../dist/linux/lib -lopenpano -lstdc++ -lpthread -lm
#cgo windows LDFLAGS: -L${SRCDIR}/../../dist/windows/lib -lopenpano -lstdc++

#include "openpano_ffi.h"
#include <stdlib.h>
*/
import "C"

import (
	"errors"
	"runtime"
	"unsafe"
)

// StitchMode represents the stitching algorithm mode.
type StitchMode int

const (
	// ModeCylinder uses cylindrical projection for camera rotation around vertical axis.
	// Best for standard panoramas. Requires ordered input and known focal length.
	ModeCylinder StitchMode = C.OPENPANO_MODE_CYLINDER

	// ModeEstimateCamera uses bundle adjustment for arbitrary camera poses.
	// Most flexible mode, works with unordered images.
	ModeEstimateCamera StitchMode = C.OPENPANO_MODE_ESTIMATE_CAMERA

	// ModeTranslation assumes pure translation between images.
	// Best for document scanning or satellite imagery. Requires ordered input.
	ModeTranslation StitchMode = C.OPENPANO_MODE_TRANSLATION
)

// LogLevel controls the verbosity of logging output.
type LogLevel int

const (
	// LogNone disables all logging.
	LogNone LogLevel = C.OPENPANO_LOG_NONE
	// LogError shows only error messages.
	LogError LogLevel = C.OPENPANO_LOG_ERROR
	// LogWarn shows warnings and errors.
	LogWarn LogLevel = C.OPENPANO_LOG_WARN
	// LogInfo shows informational messages, warnings, and errors.
	LogInfo LogLevel = C.OPENPANO_LOG_INFO
	// LogDebug shows all messages including debug output.
	LogDebug LogLevel = C.OPENPANO_LOG_DEBUG
)

// Error types returned by OpenPano functions.
var (
	// ErrInvalidHandle indicates an invalid or null handle was provided.
	ErrInvalidHandle = errors.New("openpano: invalid handle")
	// ErrInvalidArgument indicates an invalid argument or parameter value.
	ErrInvalidArgument = errors.New("openpano: invalid argument")
	// ErrFileNotFound indicates the specified file could not be found.
	ErrFileNotFound = errors.New("openpano: file not found")
	// ErrInsufficientImages indicates fewer than 2 images were provided.
	ErrInsufficientImages = errors.New("openpano: need at least 2 images")
	// ErrStitchingFailed indicates the stitching operation failed.
	ErrStitchingFailed = errors.New("openpano: stitching failed")
	// ErrOutOfMemory indicates memory allocation failed.
	ErrOutOfMemory = errors.New("openpano: out of memory")
	// ErrFeatureDetection indicates feature detection failed.
	ErrFeatureDetection = errors.New("openpano: feature detection failed")
	// ErrMatching indicates feature matching failed.
	ErrMatching = errors.New("openpano: feature matching failed")
	// ErrUnsupportedFormat indicates the image format is not supported.
	ErrUnsupportedFormat = errors.New("openpano: unsupported format")
	// ErrCancelled indicates the operation was cancelled.
	ErrCancelled = errors.New("openpano: operation cancelled")
	// ErrUnknown indicates an unknown error occurred.
	ErrUnknown = errors.New("openpano: unknown error")
)

// errorFromC converts a C error code to a Go error.
func errorFromC(err C.OpenpanoError) error {
	switch err {
	case C.OPENPANO_OK:
		return nil
	case C.OPENPANO_ERROR_INVALID_HANDLE:
		return ErrInvalidHandle
	case C.OPENPANO_ERROR_INVALID_ARGUMENT:
		return ErrInvalidArgument
	case C.OPENPANO_ERROR_FILE_NOT_FOUND:
		return ErrFileNotFound
	case C.OPENPANO_ERROR_INSUFFICIENT_IMAGES:
		return ErrInsufficientImages
	case C.OPENPANO_ERROR_STITCHING_FAILED:
		return ErrStitchingFailed
	case C.OPENPANO_ERROR_OUT_OF_MEMORY:
		return ErrOutOfMemory
	case C.OPENPANO_ERROR_FEATURE_DETECTION_FAILED:
		return ErrFeatureDetection
	case C.OPENPANO_ERROR_MATCHING_FAILED:
		return ErrMatching
	case C.OPENPANO_ERROR_UNSUPPORTED_FORMAT:
		return ErrUnsupportedFormat
	case C.OPENPANO_ERROR_CANCELLED:
		return ErrCancelled
	default:
		return ErrUnknown
	}
}

// Config holds configuration options for stitching.
type Config struct {
	// Mode specifies the stitching algorithm mode.
	Mode StitchMode

	// FocalLength is the focal length in 35mm equivalent.
	// Used primarily in cylinder mode. Typical values: 24-200mm.
	FocalLength float32

	// OrderedInput indicates whether images are in sequential order.
	// When true, assumes left-to-right or right-to-left ordering.
	OrderedInput bool

	// CropResult enables cropping to remove black borders.
	CropResult bool

	// NumThreads sets the number of threads (0 = auto-detect).
	NumThreads int

	// LazyRead enables on-demand image loading to reduce memory.
	LazyRead bool

	// Straighten enables perspective correction.
	Straighten bool

	// MaxOutputSize limits the output dimension (0 = no limit).
	MaxOutputSize int

	// Multiband sets the number of bands for blending (0 = disabled).
	Multiband int
}

// DefaultConfig returns a Config with sensible default values.
func DefaultConfig() Config {
	var c C.OpenpanoConfig
	C.openpano_config_default(&c)

	return Config{
		Mode:          StitchMode(c.mode),
		FocalLength:   float32(c.focal_length),
		OrderedInput:  c.ordered_input != 0,
		CropResult:    c.crop_result != 0,
		NumThreads:    int(c.num_threads),
		LazyRead:      c.lazy_read != 0,
		Straighten:    c.straighten != 0,
		MaxOutputSize: int(c.max_output_size),
		Multiband:     int(c.multiband),
	}
}

// toC converts a Go Config to a C OpenpanoConfig.
func (c Config) toC() C.OpenpanoConfig {
	ordered := C.int32_t(0)
	if c.OrderedInput {
		ordered = 1
	}
	crop := C.int32_t(0)
	if c.CropResult {
		crop = 1
	}
	lazy := C.int32_t(0)
	if c.LazyRead {
		lazy = 1
	}
	straighten := C.int32_t(0)
	if c.Straighten {
		straighten = 1
	}

	return C.OpenpanoConfig{
		mode:            C.OpenpanoMode(c.Mode),
		focal_length:    C.float(c.FocalLength),
		ordered_input:   ordered,
		crop_result:     crop,
		num_threads:     C.int32_t(c.NumThreads),
		lazy_read:       lazy,
		straighten:      straighten,
		max_output_size: C.int32_t(c.MaxOutputSize),
		multiband:       C.int32_t(c.Multiband),
	}
}

// ProgressFunc is called during stitching to report progress.
// progress is a value from 0.0 to 1.0, and stage describes the current operation.
type ProgressFunc func(progress float32, stage string)

// Stitcher manages panorama stitching operations.
type Stitcher struct {
	handle C.OpenpanoStitcherHandle
}

// NewStitcher creates a new stitcher with the given configuration.
func NewStitcher(config Config) (*Stitcher, error) {
	cConfig := config.toC()
	handle := C.openpano_stitcher_create(&cConfig)
	if handle == nil {
		return nil, ErrOutOfMemory
	}

	s := &Stitcher{handle: handle}
	runtime.SetFinalizer(s, (*Stitcher).Close)
	return s, nil
}

// NewDefaultStitcher creates a stitcher with default configuration.
func NewDefaultStitcher() (*Stitcher, error) {
	return NewStitcher(DefaultConfig())
}

// Close releases resources associated with the stitcher.
// It's safe to call Close multiple times.
func (s *Stitcher) Close() {
	if s.handle != nil {
		C.openpano_stitcher_destroy(s.handle)
		s.handle = nil
	}
}

// AddImage adds an image file to the stitcher.
// Supported formats depend on build configuration (typically JPEG and PNG).
func (s *Stitcher) AddImage(path string) error {
	if s.handle == nil {
		return ErrInvalidHandle
	}

	cPath := C.CString(path)
	defer C.free(unsafe.Pointer(cPath))

	err := C.openpano_add_image_file(s.handle, cPath)
	return errorFromC(err)
}

// AddImageData adds an image from raw RGB pixel data.
// pixels should contain width * height * 3 bytes in RGB order.
func (s *Stitcher) AddImageData(pixels []byte, width, height int) error {
	if s.handle == nil {
		return ErrInvalidHandle
	}
	if len(pixels) == 0 {
		return ErrInvalidArgument
	}

	err := C.openpano_add_image_data(
		s.handle,
		(*C.uint8_t)(unsafe.Pointer(&pixels[0])),
		C.int32_t(width),
		C.int32_t(height),
		0,
	)
	return errorFromC(err)
}

// AddImageDataFloat adds an image from float RGB pixel data.
// pixels should contain width * height * 3 floats in [0,1] range.
func (s *Stitcher) AddImageDataFloat(pixels []float32, width, height int) error {
	if s.handle == nil {
		return ErrInvalidHandle
	}
	if len(pixels) == 0 {
		return ErrInvalidArgument
	}

	err := C.openpano_add_image_data_float(
		s.handle,
		(*C.float)(unsafe.Pointer(&pixels[0])),
		C.int32_t(width),
		C.int32_t(height),
		0,
	)
	return errorFromC(err)
}

// ImageCount returns the number of images added to the stitcher.
func (s *Stitcher) ImageCount() int {
	if s.handle == nil {
		return 0
	}
	count := C.openpano_image_count(s.handle)
	if count < 0 {
		return 0
	}
	return int(count)
}

// Clear removes all added images.
func (s *Stitcher) Clear() error {
	if s.handle == nil {
		return ErrInvalidHandle
	}
	err := C.openpano_clear_images(s.handle)
	return errorFromC(err)
}

// Stitch executes panorama stitching and returns the result image.
// This is a blocking operation that may take significant time.
func (s *Stitcher) Stitch() (*Image, error) {
	if s.handle == nil {
		return nil, ErrInvalidHandle
	}

	var result C.OpenpanoImageHandle
	err := C.openpano_stitch(s.handle, &result)
	if err != C.OPENPANO_OK {
		return nil, errorFromC(err)
	}

	img := &Image{handle: result}
	runtime.SetFinalizer(img, (*Image).Close)
	return img, nil
}

// Cancel aborts an ongoing stitching operation.
// Can be called from another goroutine.
func (s *Stitcher) Cancel() error {
	if s.handle == nil {
		return ErrInvalidHandle
	}
	err := C.openpano_cancel(s.handle)
	return errorFromC(err)
}

// Image represents a panorama result image.
type Image struct {
	handle C.OpenpanoImageHandle
}

// Close releases resources associated with the image.
// It's safe to call Close multiple times.
func (img *Image) Close() {
	if img.handle != nil {
		C.openpano_image_destroy(img.handle)
		img.handle = nil
	}
}

// Dimensions returns the image width and height in pixels.
func (img *Image) Dimensions() (width, height int, err error) {
	if img.handle == nil {
		return 0, 0, ErrInvalidHandle
	}

	var w, h C.int32_t
	cerr := C.openpano_image_dimensions(img.handle, &w, &h)
	if cerr != C.OPENPANO_OK {
		return 0, 0, errorFromC(cerr)
	}
	return int(w), int(h), nil
}

// Width returns the image width in pixels.
func (img *Image) Width() int {
	w, _, _ := img.Dimensions()
	return w
}

// Height returns the image height in pixels.
func (img *Image) Height() int {
	_, h, _ := img.Dimensions()
	return h
}

// Channels returns the number of color channels (typically 3 for RGB).
func (img *Image) Channels() int {
	if img.handle == nil {
		return 0
	}
	ch := C.openpano_image_channels(img.handle)
	if ch < 0 {
		return 0
	}
	return int(ch)
}

// Pixels returns a copy of the pixel data as RGB bytes.
// The data is in row-major order with 3 bytes per pixel.
func (img *Image) Pixels() ([]byte, error) {
	if img.handle == nil {
		return nil, ErrInvalidHandle
	}

	var pixels *C.uint8_t
	var size C.size_t

	err := C.openpano_image_data(img.handle, &pixels, &size)
	if err != C.OPENPANO_OK {
		return nil, errorFromC(err)
	}

	// Copy to Go slice
	result := make([]byte, size)
	copy(result, (*[1 << 30]byte)(unsafe.Pointer(pixels))[:size:size])
	return result, nil
}

// PixelsFloat returns a copy of the pixel data as RGB floats.
// The data is in row-major order with 3 floats per pixel in [0,1] range.
func (img *Image) PixelsFloat() ([]float32, error) {
	if img.handle == nil {
		return nil, ErrInvalidHandle
	}

	var pixels *C.float
	var size C.size_t

	err := C.openpano_image_data_float(img.handle, &pixels, &size)
	if err != C.OPENPANO_OK {
		return nil, errorFromC(err)
	}

	count := int(size) / 4 // sizeof(float)
	result := make([]float32, count)
	copy(result, (*[1 << 28]float32)(unsafe.Pointer(pixels))[:count:count])
	return result, nil
}

// Save writes the image to a file.
// Format is determined by extension: .jpg/.jpeg for JPEG, .png for PNG.
// quality is used for JPEG compression (1-100, higher = better quality).
func (img *Image) Save(path string, quality int) error {
	if img.handle == nil {
		return ErrInvalidHandle
	}

	cPath := C.CString(path)
	defer C.free(unsafe.Pointer(cPath))

	err := C.openpano_image_save(img.handle, cPath, C.int32_t(quality))
	return errorFromC(err)
}

// Version returns the OpenPano library version string.
func Version() string {
	return C.GoString(C.openpano_version())
}

// HasJPEGSupport returns true if JPEG format is supported.
func HasJPEGSupport() bool {
	return C.openpano_has_jpeg_support() != 0
}

// HasPNGSupport returns true if PNG format is supported.
func HasPNGSupport() bool {
	return C.openpano_has_png_support() != 0
}

// CPUCount returns the number of available CPU cores.
func CPUCount() int {
	return int(C.openpano_get_cpu_count())
}

// SetLogLevel sets the global logging verbosity.
func SetLogLevel(level LogLevel) {
	C.openpano_set_log_level(C.OpenpanoLogLevel(level))
}

