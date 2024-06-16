#!/usr/bin/env python

import glob
import struct
import numpy
import sys, json
import argparse
from PIL import Image
from os.path import basename
from collections import OrderedDict
from operator import itemgetter

def save_binary_data(f_data, f, width, height, padding=True):
	# pad to next 256byte boundary?
	size = len(f_data)

	height = (height + 7) // 8

	for b in range(size):
		f.write(struct.pack('<B', f_data[b]))

	pad = 256 - (size % 256)
	if pad != 256 and padding:
		f.write(struct.pack('<B', 0) * pad)
	else:
		pad = 0

	return size + pad

def convert_image(width, height, data, color):
	# calculation size of resulting image in bytes
	size = (width * ((height + 7) // 8 * 8)) // 8
	f_data = []
	for b in range(size):
		f_data.append(0)
	#
	# Input image is a linear list of elements, each representing 1 pixel.
	# The input data it organized width wise, e.g. each 'width' elements
	# represent one row of the input image.
	#
	# Linear data organized in rows:
	#    [ r1 ][ r2 ][ r3 ][ r4 ]
	#
	# Rows logically organized as image:
	#   [ r1 ]
	#   [ r2 ]
	#   [ r3 ]
	#   [ r4 ]
	#

	#
	# each byte of the output data can hold 8 rows (8 pixel)
	# calculate how many rows fit into the height of the image
	#
	byte_rows = (height + 7) // 8
	o = 0

	#
	# this loop converts the input data from row-major to column-major
	# representation
	#
	for w in range(width):
		for byte_row in range(byte_rows):
			for h in range(8):
				if (h + byte_row * 8) >= height:
					break
				c = data[(h + byte_row * 8) * width + w]
				if c > color:
					f_data[o] = f_data[o] | (0x1 << h)
			o += 1

	return f_data

def write_image_as_comment(width, height, data, frame, f, color):
	f.write("/* [%u]" % frame)
	for h in range(height):
		f.write("\n * ")
		for w in range(width):
			c = data[(h * (width + 0)) + w]
			if c > color:
				f.write("*")
			else:
				f.write("_")
	f.write("\n */\n")

def usage():
	print("...")

def decode_json(jdata):
	frames = len(jdata["frames"])
	filename = jdata["meta"]["image"]
	imageWidth = jdata["meta"]["size"]["w"]
	imageHeight = jdata["meta"]["size"]["h"]
	fdata = list(jdata["frames"].values())[0]
	frameWidth = fdata["spriteSourceSize"]["w"]
	frameHeight = fdata["spriteSourceSize"]["h"]
	# assume all sprites have the same dimension
	return [filename, frames, imageWidth, imageHeight, frameWidth, frameHeight]

outputfilename = "images"

images = OrderedDict()

if __name__ == "__main__":
	flashOffset = 0
	total_size = 0

	parser = argparse.ArgumentParser()
	parser.add_argument("artifacts_dir", help="path to the artifacts")
	parser.add_argument("-i", "--interleave", type=int, choices=[2], default=1, help="increase output verbosity")
	args = parser.parse_args()

	args.artifacts_dir += "/"

	for json_filename in sorted(glob.glob(args.artifacts_dir + "*.json")):
		img_name = basename(json_filename).split('.')[0].replace('-', '_')
		jdata = None
		with open(json_filename) as jdatafile:
			jdata = json.load(jdatafile, object_pairs_hook=OrderedDict)
		if not jdata:
			continue

		filename, frames, imageWidth, imageHeight, frameWidth, frameHeight = decode_json(jdata)

		frameHeight *= args.interleave
		frames //= args.interleave

		im = Image.open(args.artifacts_dir + filename)
		palette= im.getpalette()
		fdata = list(im.getdata())

		# create array of 'height' rows each with 'width' elements [ [width elemets] * height_times ]
		a = numpy.reshape(numpy.asarray(fdata), (imageHeight, imageWidth))

		size = ((imageHeight + 7) // 8) * imageWidth

		color = 0
		print("%-40s" % (basename(filename)), end=' ')

		# print some information
		print("  img width: %3u img height: %3u" % (imageWidth, imageHeight), end=' ')
		print("  frame width: %3u frame height: %3u" % (frameWidth, frameHeight), end=' ')
		print("  frames: %3u" % (frames), end=' ')
		print("  size: %5u" % (size))

		total_size += size

		images[img_name] = {"info": (basename(filename), size, frameHeight, frameWidth)}
		images[img_name]["raw"] = {}
		images[img_name]["target"] = {}

		foffset = 0
		previous_frame = None
		for frame_nr in range(frames):
			foffset = frame_nr * frameWidth
			# copy each image and reshape to linear list
			# select row and number of rows by slicing, then convert to linear list
			img = numpy.reshape(a[frame_nr * frameHeight: frame_nr * frameHeight + frameHeight], frameWidth*frameHeight).tolist()
			images[img_name]["raw"][frame_nr] = img
			frame = convert_image(frameWidth, frameHeight, img, color)
			images[img_name]["target"][frame_nr] = frame

	for k, v in images.items():
		h = v["info"][2]
		w = v["info"][3]

		# sum up total image size from all frames
		isize = 0
		for k2, v2 in v["target"].items():
			inc = len(v["target"][k2])
			isize += inc

		binFileName = v["info"][0][:-4] + ".bin"
		txtFileName = v["info"][0][:-4] + ".txt"
		with open(binFileName, 'wb') as binFile, open(txtFileName, 'w') as txtFile:
			for k2, v2 in v["raw"].items():
				# save textual image information
				txtFile.write("\n/* %s height = %u width = %u */\n" % (binFileName, h, w))
				write_image_as_comment(w, h, v2, k2, txtFile, color)
				# save binary file with image data
				save_binary_data(v["target"][k2], binFile, w, h, False)

	print("total image data         = %u bytes" % total_size)
