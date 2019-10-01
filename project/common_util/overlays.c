// Copyright (c) 2019 Xnor.ai, Inc.
//
#include "overlays.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum
{
	LINE_WIDTH = OVERLAY_TEXT_SIZE / 8
};

static const char *const FONT = "Courier";

xg_overlay *xg_overlay_create_bounding_box(float x, float y, float width,
					   float height, const char *label,
					   xg_color color)
{
	xg_overlay *result = calloc(1, sizeof(xg_overlay));
	if (result == NULL) {
		return NULL;
	}
	result->type = XG_OVERLAY_BOUNDING_BOX;
	result->x = x;
	result->y = y;
	result->width = width;
	result->height = height;
	result->text = strdup(label);
	if (result->text == NULL) {
		free(result);
		return NULL;
	}
	result->bg_color = color;
	result->text_color = (xg_color){0, 0, 0, 255};
	return result;
}

xg_overlay *xg_overlay_create_text(float x, float y, const char *label,
				   xg_color color)
{
	xg_overlay *result = calloc(1, sizeof(xg_overlay));
	if (result == NULL)
	{
		return NULL;
	}
	result->type = XG_OVERLAY_TEXT;
	result->x = x;
	result->y = y;
	result->text = strdup(label);
	if (result->text == NULL)
	{
		free(result);
		return NULL;
	}
	result->bg_color = color;
	result->text_color = (xg_color){0, 0, 0, 255};
	return result;
}

void xg_overlay_free(xg_overlay *overlay)
{
	free(overlay->text);
	free(overlay);
}

static void xg_overlay_text_draw(xg_overlay *text, cairo_t *cr,
				 int32_t surface_width,
				 int32_t surface_height)
{
	// Absolute position of upper left corner of the current line
	double x = text->x * surface_width + LINE_WIDTH;
	double y = text->y * surface_height + LINE_WIDTH;

	cairo_push_group(cr);
	cairo_move_to(cr, x + LINE_WIDTH / 2.0,
		      y + LINE_WIDTH * 1.5 + OVERLAY_TEXT_SIZE / 2.0);
	cairo_set_source_rgba(
	    cr, text->text_color.r / 255.0f, text->text_color.g / 255.0f,
	    text->text_color.b / 255.0f, text->text_color.a / 255.0f);
	cairo_set_font_size(cr, OVERLAY_TEXT_SIZE);
	cairo_select_font_face(cr, FONT, CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_BOLD);
	cairo_show_text(cr, text->text);
	double cur_x, cur_y;
	cairo_get_current_point(cr, &cur_x, &cur_y);
	double line_width = cur_x - x;
	double line_height = cur_y - y;
	cairo_pattern_t *text_surface = cairo_pop_group(cr);

	cairo_set_source_rgba(cr, text->bg_color.r / 255.0f,
			      text->bg_color.g / 255.0f, text->bg_color.b / 255.0f,
			      text->bg_color.a / 255.0f);
	cairo_set_line_width(cr, 2);
	cairo_rectangle(cr, x, y, line_width + LINE_WIDTH / 2.0,
			line_height + LINE_WIDTH);
	cairo_fill(cr);

	cairo_set_source(cr, text_surface);
	cairo_paint(cr);

	cairo_pattern_destroy(text_surface);
}

static void xg_overlay_bounding_box_draw(xg_overlay *bounding_box, cairo_t *cr,
					 int32_t surface_width,
					 int32_t surface_height)
{
	// Transform relative positioning to absolute
	double width = bounding_box->width * surface_width;
	double height = bounding_box->height * surface_height;
	double x = bounding_box->x * surface_width;
	double y = bounding_box->y * surface_height;

	// Make space for the border
	x += LINE_WIDTH;
	y += LINE_WIDTH;

	cairo_set_line_width(cr, LINE_WIDTH);
	cairo_set_source_rgba(
	    cr, bounding_box->bg_color.r / 255.0f, bounding_box->bg_color.g / 255.0f,
	    bounding_box->bg_color.b / 255.0f, bounding_box->bg_color.a / 255.0f);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
	cairo_rectangle(cr, x, y, width, height);
	cairo_stroke(cr);

	xg_overlay_text_draw(bounding_box, cr, surface_width, surface_height);
}

void xg_overlay_draw(xg_overlay *overlay, cairo_t *cr, int32_t surface_width,
		     int32_t surface_height)
{
	switch (overlay->type)
	{
	case XG_OVERLAY_BOUNDING_BOX:
		{
			xg_overlay_bounding_box_draw(overlay, cr, surface_width, surface_height);
			break;
		}
	case XG_OVERLAY_TEXT:
		{
			xg_overlay_text_draw(overlay, cr, surface_width, surface_height);
			break;
		}
	}
}
