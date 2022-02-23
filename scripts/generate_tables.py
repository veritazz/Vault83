#!/usr/bin/python

import math, struct
from collections import OrderedDict

blockSize = 64
distanceToProjectionPlane = 102
fogOfWarDistance = 512
byX = 64
tanByX = 64
fineByX = 65536

def save_binary_data(f_data, f, padding=True):
	# pad to next 256byte boundary?
	size = len(f_data)
	for m in range(size):
		f.write(struct.pack('<B', mtranslation[f_data[m]]))

	pad = 256 - (size % 256)
	if pad != 256 and padding:
		f.write(struct.pack('<B', 0) * pad)
	else:
		pad = 0

	return size + pad

if __name__ == "__main__":
	with open("tables.c", 'w') as cfile, open("tables.h", 'w') as hfile:
		hfile.write("#ifndef __TABLES_H\n#define __TABLES_H\n\n#include <stdint.h>\n\n")
		hfile.write("extern const int8_t cosByX[360];\n");
		hfile.write("extern const uint16_t fineCosByX[91];\n");
		hfile.write("extern const uint16_t tanByX[90];\n");
		hfile.write("extern const uint16_t stepRaylength[91];\n");
		hfile.write("extern const uint8_t hopsPerAngle[91];\n");
		hfile.write("extern const uint8_t pixel_patterns[768];\n");
		hfile.write("\n");
		hfile.write("#define DIVIDE_BY_X           %u\n" % (byX));
		hfile.write("#define FINE_BY_X             %u\n" % (fineByX));
		hfile.write("#define FOG_OF_WAR_DISTANCE   %u\n" % (fogOfWarDistance));


		cfile.write("#include <Arduino.h>\n")
		cfile.write("#include <stdint.h>\n\n")


		print("generating hop table...", end=' ')
		cfile.write("const uint8_t hopsPerAngle[91] PROGMEM = {");
		for i in range(91):
			if i % 10 == 0:
				cfile.write("\n\t");
			else:
				cfile.write(" ");
			if i != 0 and i != 90:
				step = int(round(byX / math.cos(math.radians(i))))
				cfile.write("%2d," % ((fogOfWarDistance + step - 1) / step))
			else:
				cfile.write("%2d," % (8))
		cfile.write("\n};\n");
		print("done")

		print("generating step table...", end=' ')
		cfile.write("const uint16_t stepRaylength[91] PROGMEM = {");
		for i in range(91):
			if i % 10 == 0:
				cfile.write("\n\t");
			else:
				cfile.write(" ");
			if i != 0 and i != 90:
				cfile.write("%4d," % (int(round(byX / math.cos(math.radians(i))))))
			else:
				cfile.write("%4d," % (byX))
		cfile.write("\n};\n");
		print("done")

		#
		# cosByX
		#
		print("generating cosinus table...", end=' ')
		cfile.write("\n");
		cfile.write("const int8_t cosByX[360] PROGMEM = {");
		for i in range(360):
			if i % 10 == 0:
				cfile.write("\n\t");
			else:
				cfile.write(" ");
			cfile.write("%4d," % (int(round(math.cos(math.radians(i)) * byX))))
		cfile.write("\n};\n");
		print("done")


		#
		# fine cosByX
		#
		print("generating fine cosinus table...", end=' ')
		cfile.write("\n");
		cfile.write("const uint16_t fineCosByX[91] PROGMEM = {");
		for i in range(91):
			if i % 10 == 0:
				cfile.write("\n\t");
			else:
				cfile.write(" ");
			value = int(round(math.cos(math.radians(i)) * fineByX))
			if value > 65535:
				value = 65535
			cfile.write("%4d," % (value))
		cfile.write("\n};\n");
		print("done")


		#
		# tanByX
		#
		print("generating tangens table...", end=' ')
		cfile.write("\n");
		cfile.write("const uint16_t tanByX[90] PROGMEM = {");
		for i in range(90):
			if i % 10 == 0:
				cfile.write("\n\t");
			else:
				cfile.write(" ");
			cfile.write("%4d," % (int(round(math.tan(math.radians(i)) * tanByX))))
		cfile.write("\n};\n");
		print("done")

		#
		# pixel draw tables
		#
		print("generating pixel pattern table...", end=' ')
		cfile.write("\n");
		cfile.write("/*\n * table to support drawing 1-16 pixels at once\n */\n");
		cfile.write("const uint8_t pixel_patterns[768] PROGMEM = {\n");
		cfile.write("/*" + 13 * " " + "|")
		for i in range(8):
			cfile.write(7 * " " + " shift by %d     |" % i)
		cfile.write(" */")
		for pixels in (0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff):
			cfile.write("\n/* %2d pixel%s */\t" % (bin(pixels).count("1"), " " if pixels == 1 else "s"))
			for i in range(8):
				cfile.write(" %#2.2x, %#2.2x, %#2.2x, %#2.2x," % (pixels & 0xff, pixels >> 8 & 0xff, 0, 0))
				pixels <<= 1

		for pixels in (0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff):
			cfile.write("\n/* %2d pixels */\t" % (bin(pixels).count("1")))
			for i in range(8):
				cfile.write(" %#2.2x, %#2.2x, %#2.2x, %#2.2x," % (pixels & 0xff, pixels >> 8 & 0xff, pixels >> 16 & 0xff, 0))
				pixels <<= 1

		for pixels in (0x01ffff, 0x03ffff, 0x07ffff, 0x0fffff, 0x1fffff, 0x3fffff, 0x7fffff, 0xffffff):
			cfile.write("\n/* %2d pixels */\t" % (bin(pixels).count("1")))
			for i in range(8):
				cfile.write(" %#2.2x, %#2.2x, %#2.2x, %#2.2x," % (pixels & 0xff, pixels >> 8 & 0xff, pixels >> 16 & 0xff, pixels >> 24 & 0xff))
				pixels <<= 1

		cfile.write("\n};\n")
		print("done")

		print("generating raylengths table...", end=' ')
		newPatterns = OrderedDict()
		f = 0.0
		maxD = 0
		for wh in range(1, 700000):
			whc = 1.0 * wh / 100
			s = 32.0 / whc
			f = 0.0
			key = ""
			v = 1
			x = 0
			pxBase = 1.0 / s
			nrOfHash = 0
			for t in range(32):
				p = f + 1.0 / s
				f = p - int(p)
				if int(p) > int(pxBase):
					key += '#'
					x += v
					nrOfHash += 1
				else:
					key += 'x'
				v <<= 1
			elm = 'x'
			if nrOfHash > 16:
				elm = '#'

			first = 0
			fv = 32
			lv = 0
			for c in key:
				if c == elm:
					lv += 1
				else:
					if not first:
						first = 1
					lv = 0

			d = 0
			if fv > lv:
				d = fv - lv
			else:
				d = lv - fv

			if d > maxD:
				maxD = d

			newPatterns[whc] = (int(2048 * s), x, int(pxBase), key, fv, lv, d)

		#
		# write out binary data
		#
		# table is indexed by wallHeight -> [scale, px_base, [4 byte pattern]]
		# table is indexed by raylength -> [wallHeight, scale, px_base, [4 byte pattern]]
		#
		with open("rayLengths.bin", 'wb') as bfile:
			wallHeight = blockSize * distanceToProjectionPlane
			for rl in range(fogOfWarDistance + 1):
				wht = wallHeight
				wh = 1.0 * wallHeight
				if rl:
					wht = wallHeight / rl
					wh /= rl

				if wh > 1:
					wh -= 1
				if wht > 1:
					wht -= 1

				whf = round(wh, 2)
				scale = newPatterns[whf][0]

				px_base = newPatterns[whf][2]

				bfile.write(struct.pack('<H', round(whf))) # wallheight
				bfile.write(struct.pack('<H', scale)) # scale
				bfile.write(struct.pack('<B', px_base)) # px_base
				pattern = newPatterns[whf][1]

				key = newPatterns[whf][3]

				bfile.write(struct.pack('<I', pattern))
				#print("%f %4u %3u %s fv %2u lv %2u d %d" % (whf, scale, px_base, key, newPatterns[whf][4], newPatterns[whf][5], newPatterns[whf][6]))
		print("done")

		print("generating distance table...", end=' ')
		with open("distances.bin", 'wb') as bfile:
			for dx in range(272):
				for dy in range (272):
					# distance to the sprite
					distance = int(round(math.sqrt((dx*dx) + (dy*dy))))
					# minimum angle left and right of the field of view the sprite is possibly
					# visible. This will later be used by the engine to determine if any part
					# of the sprite is in the field of view
					minAngle = 0
					if distance >= 8:
						minAngle = int(round(math.degrees(math.acos(16.0 / 2 / distance))))
					minAngle %= 90

					if dy > dx:
						tan = dy
						if dx > 1:
							tan /= dx
						qAngle = 90 - int(round(math.degrees(math.atan(tan))))
					else:
						tan = dx
						if dy > 1:
							tan /= dy
						qAngle = int(round(math.degrees(math.atan(tan))))
					# qAngle: angle in the current quadrant. It is later used to calculate the
					#         full angle of the sprite
					bfile.write(struct.pack('<HBB', distance, minAngle, qAngle))
		print("done")

		hfile.write("\n");
		hfile.write("\n");
		hfile.write("\n#endif");
