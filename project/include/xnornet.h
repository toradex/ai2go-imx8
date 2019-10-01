// Copyright (c) 2019 Xnor.ai, Inc.
//
// Defines the Xnor external C API.
//
#ifndef PUBLIC_XNORNET_API_INCLUDED
#define PUBLIC_XNORNET_API_INCLUDED

#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__) && __GNUC__ >= 4
#define XNOR_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define XNOR_WARN_UNUSED_RESULT
#endif

#ifdef __cplusplus
extern "C"
{
#endif // defined(__cplusplus)

	// Opaque handle to an error that has occurred.
	typedef struct xnor_error xnor_error;

	// Opaque handle to input for an xnor model.
	typedef struct xnor_input xnor_input;

	// Opaque handle to a set of options for loading an xnor model.
	typedef struct xnor_model_load_options xnor_model_load_options;

	// Opaque handle to an xnor model.
	typedef struct xnor_model xnor_model;

	// Opaque handle to an evaluation result.
	typedef struct xnor_evaluation_result xnor_evaluation_result;

	// Types of evaluation results.  New values may be added to this enumeration in
	// the future; for forward compatibility, treat any values not listed here as
	// unknown.
	typedef enum xnor_evaluation_result_type
	{
		kXnorEvaluationResultTypeUnknown,
		kXnorEvaluationResultTypeBoundingBoxes,
		kXnorEvaluationResultTypeClassLabels,
		kXnorEvaluationResultTypeSegmentationMasks
	} xnor_evaluation_result_type;

	// Information about an xnor_model
	typedef struct xnor_model_info
	{
		// Size of this struct.  Caller must set. (Allows versioning of this struct.)
		size_t xnor_model_info_size;
		// A friendly name for the model.
		const char *name;
		// The type of result evaluate() will return.
		xnor_evaluation_result_type result_type;
		// Model version information
		const char *version;
		// For models with category labels, which category labels can be returned from
		// this model.
		int num_class_labels;
		const char *const *class_labels;
	} xnor_model_info;

	// Type for describing threading model of inference engine execution.  New
	// values may be added to this enumeration in the future; for forward
	// compatibility treat any values not listed here as unknown.
	typedef enum xnor_threading_model
	{
		kXnorThreadingModelMultiThreaded = 1, // Allow use of more than one core
		kXnorThreadingModelSingleThreaded,    // Use one core only
	} xnor_threading_model;

	// A rectangle in the Cartesian coordinate system.
	typedef struct xnor_rectangle
	{
		float x, y, width, height;
	} xnor_rectangle;

	// A single prediction of a class label.
	typedef struct xnor_class_label
	{
		int32_t class_id;
		const char *label; // valid only while evaluation result not freed
	} xnor_class_label;

	// A bounding box and color annotated with a class label.
	typedef struct xnor_bounding_box
	{
		xnor_class_label class_label;
		xnor_rectangle rectangle;
	} xnor_bounding_box;

	// A 1-bit data map
	typedef struct xnor_bitmap
	{
		// Width of the populated data, given in bits.  May not be a multiple of the
		// row alignment, in which case the high-order bits of the last byte will be
		// "padded" with 0 and additional padding bytes filled with 0 may be present.
		int32_t width;
		// Number of rows in the bitmap. The top row comes first.
		int32_t height;
		// Offset, in bytes, from one row of the mask to the next.
		int32_t stride;
		// A buffer of size @stride * @height.  Within each byte, moving from the
		// least significant bit to the most significant bit corresponds to advancing
		// by column through the image.  Only valid until evaluation result freed.
		const uint8_t *data;
	} xnor_bitmap;

	// A mask indicating which pixels in the input image match a particular class.
	// A bit in @bitmap is set if the area of the image corresponding to the (x,y)
	// coordinate of the bit is of the class given by @class_label. The mask
	// bitmap's dimensions may be smaller or larger and in a different aspect ratio
	// than the input image.
	typedef struct xnor_segmentation_mask
	{
		xnor_class_label class_label;
		xnor_bitmap bitmap;
	} xnor_segmentation_mask;

	// Given an error, gets a description of it.  The returned string is valid only
	// while the error has not been freed.
	const char *xnor_error_get_description(xnor_error *error);

	// Frees the error.  This invalidates any description string returned
	// previously from this error.
	void xnor_error_free(xnor_error *error);

	// Creates a handle to an image input, where the image is provided as a JPEG.
	// The data must remain valid until the input handle is destroyed.
	xnor_error *xnor_input_create_jpeg_image(
	    const uint8_t *data, int32_t data_length,
	    xnor_input **result) XNOR_WARN_UNUSED_RESULT;

	// Creates a handle to an image input, where the image is interleaved RGB
	// bytes in row-major left-to-right top-down order.  The data pointer must
	// remain valid until the input handle is destroyed.
	xnor_error *xnor_input_create_rgb_image(
	    int32_t width, int32_t height, const uint8_t *data,
	    xnor_input **result) XNOR_WARN_UNUSED_RESULT;

	// Creates a handle to an image input, where the image is interleaved YUV422
	// bytes in row-major left-to-right top-down order.  The data pointer must
	// remain valid until the input handle is destroyed.
	xnor_error *xnor_input_create_yuv422_image(
	    int32_t width, int32_t height, const uint8_t *data,
	    xnor_input **result) XNOR_WARN_UNUSED_RESULT;

	// Creates a handle to an image input, where the image is planar YUV420 bytes
	// in a row-major left-to-right top-down order.  The data pointers must remain
	// valid until the input handle is destroyed.
	xnor_error *xnor_input_create_yuv420p_image(
	    int32_t width, int32_t height, const uint8_t *y_plane_data,
	    const uint8_t *u_plane_data, const uint8_t *v_plane_data,
	    xnor_input **result) XNOR_WARN_UNUSED_RESULT;

	// Creates a handle to an image input, where the image is semi-planar YUV420
	// bytes with u channel first in a row-major left-to-right top-down order.  The
	// data pointers must remain valid until the input handle is destroyed.
	xnor_error *xnor_input_create_yuv420sp_nv12_image(
	    int32_t width, int32_t height, const uint8_t *y_plane_data,
	    const uint8_t *uv_plane_data, xnor_input **result) XNOR_WARN_UNUSED_RESULT;

	// Creates a handle to an image input, where the image is semi-planar YUV420
	// bytes with v channel first in a row-major left-to-right top-down order.  The
	// data pointers must remain valid until the input handle is destroyed.
	xnor_error *xnor_input_create_yuv420sp_nv21_image(
	    int32_t width, int32_t height, const uint8_t *y_plane_data,
	    const uint8_t *vu_plane_data, xnor_input **result) XNOR_WARN_UNUSED_RESULT;

	// Frees the input.  The underlying data is not freed.
	void xnor_input_free(xnor_input *input);

	// Create a default set of model load options.  Options can then be set on this
	// object, which can then be used when loading a model.  The options object can
	// be destroyed after the load has completed.
	xnor_model_load_options *xnor_model_load_options_create(void);

	// Free a set of model load options.
	void xnor_model_load_options_free(xnor_model_load_options *load_options);

	// Set the threading model to be used by a model once it has loaded.  By
	// default models will be loaded to optimize performance by utilizing multiple
	// threads, but by changing this option you can opt instead to use only a
	// single thread at the expense of reducing performance.
	xnor_error *xnor_model_load_options_set_threading_model(
	    xnor_model_load_options *options,
	    xnor_threading_model threading_model) XNOR_WARN_UNUSED_RESULT;

	// Fills @model_names_out with up to @model_names_out_size names of the models
	// built into this bundle as null-terminated c-strings.  The lifetime of these
	// strings is static, that is, they will last until the shared object exporting
	// this function is unloaded.  Returns the total number of built in models in
	// the bundle.
	int32_t xnor_model_enumerate_built_in(const char **model_names_out,
					      int32_t model_names_out_size);

	// Loads a built-in model by name and prepares it for evaluation.  @model_name
	// identifies which model to load and can be found by calling
	// xnor_model_get_info.  @load_options are configuration parameters for this
	// inference engine/model; if NULL then defaults will be used. @result will be
	// set to the resulting model and NULL will be returned if successful;
	// otherwise, @result will be untouched and an error will be returned.
	xnor_error *xnor_model_load_built_in(
	    const char *model_name, const xnor_model_load_options *load_options,
	    xnor_model **result) XNOR_WARN_UNUSED_RESULT;

	// Retrieves information about a built in model. @model is a previously loaded
	// model.  @info should be a pointer to an xnor_model_info struct to receive the
	// information. @info.xnor_model_info_size must be filled out with the size of
	// the xnor_model_info struct.
	xnor_error *xnor_model_get_info(xnor_model *model,
					xnor_model_info *info) XNOR_WARN_UNUSED_RESULT;

	// Evaluates the given @model on the given @input. @reserved is reserved for
	// future extensibility and must be NULL.  @result will be set and NULL returned
	// on success; otherwise @result is untouched and an error is returned.
	// N.B.: xnor_model_evaluate is _not_ theadsafe: You cannot evaluate using a
	// given xnor_model* from two threads concurrently.  You _can_ load models (even
	// the same model) in multiple threads simultaneously and use each resulting
	// xnor_model* independently and concurrently.
	xnor_error *xnor_model_evaluate(xnor_model *model, const xnor_input *input,
					void *reserved, xnor_evaluation_result **result)
	    XNOR_WARN_UNUSED_RESULT;

	// Frees the given model and all associated resources.
	void xnor_model_free(xnor_model *model);

	// Gets the type of an evaluation result.
	xnor_evaluation_result_type xnor_evaluation_result_get_type(
	    xnor_evaluation_result *result);

	// Fills @out with up to @out_size bounding boxes from @result.  Returns the
	// total number of bounding boxes available in the result.  Returns -1 and
	// leaves @out untouched if the result contains a type other than bounding
	// boxes.
	int32_t xnor_evaluation_result_get_bounding_boxes(
	    xnor_evaluation_result *result, xnor_bounding_box *out, int32_t out_size);

	// Fills @out with up to @out_size class labels from @result.  Returns the
	// total number of class labels available in the result.  Returns -1 and
	// leaves @out untouched if the result contains a type other than class labels.
	// Class labels will be returned in order of descending confidence.
	int32_t xnor_evaluation_result_get_class_labels(xnor_evaluation_result *result,
							xnor_class_label *out,
							int32_t out_size);

	// Fills @out with up to @out_size segmentation masks from @result.  Returns the
	// total number of segmentation masks available in the result.  Returns -1 and
	// leaves @out untouched if the result contains a type other than segmentation
	// masks.  Segmentation masks will be returned in an arbitrary order.
	int32_t xnor_evaluation_result_get_segmentation_masks(
	    xnor_evaluation_result *result, xnor_segmentation_mask *out,
	    int32_t out_size);

	// Frees the given evaluation result and all its contents.  In particular, any
	// strings in structs obtained from the evaluation result cease to be valid,
	// and any mask data referenced by the evaluation result are no longer
	// accessible.
	void xnor_evaluation_result_free(xnor_evaluation_result *result);

#ifdef __cplusplus
} // extern "C"
#endif // defined(__cplusplus)

#undef XNOR_WARN_UNUSED_RESULT

#endif // defined(PUBLIC_XNORNET_API_INCLUDED)
