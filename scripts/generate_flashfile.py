#!/usr/bin/python

import glob
import struct
import getopt, sys
from os.path import basename
from os.path import getsize

pageSize = 256

def usage():
	print("...")

def writeConstexpr_u8(outputFile, exprName, constexpr, columnNr = 60):
	# translate unwanted characters
	inStr = "-"
	outStr = "_"
	tranStr = str.maketrans(inStr, outStr)
	# create constexpr string
	offsetName = "constexpr uint8_t %s" % (exprName.split('.')[0].translate(tranStr))
	outputHFile.write(offsetName + (" " * (columnNr - len(offsetName))) + "= 0x%2.2x;\n" % (constexpr));

def writeConstexpr_u16(outputFile, exprName, constexpr, columnNr = 60):
	# translate unwanted characters
	inStr = "-"
	outStr = "_"
	tranStr = str.maketrans(inStr, outStr)
	# create constexpr string
	offsetName = "constexpr uint16_t %s" % (exprName.split('.')[0].translate(tranStr))
	outputHFile.write(offsetName + (" " * (columnNr - len(offsetName))) + "= 0x%4.4x;\n" % (constexpr));

def writeOffset(outputFile, fileName, offset, columnNr = 60):
	# translate unwanted characters
	inStr = "-"
	outStr = "_"
	tranStr = str.maketrans(inStr, outStr)
	# create constexpr string
	offsetName = "constexpr uint24_t %s_flashoffset" % (fileName.split('.')[0].translate(tranStr))
	outputHFile.write(offsetName + (" " * (columnNr - len(offsetName))) + "= 0x%6.6x;\n" % (offset));

def getIntParameter(options, option, default):
	index = options.index(option)
	value = default
	try:
		value = int(options[index + 1], 0)
	except IndexError as ie:
		pass

	return value

def padOrAlign(outFile, options, option, fileSize, defaultPadSize):
	padSize = defaultPadSize
	pad = 0
	if option in options:
		padSize = getIntParameter(options, option, defaultPadSize)
		pad = fileSize % padSize
		if pad:
			pad = padSize - pad
			outFile.write(struct.pack('<B', 0) * pad)
	return pad

if __name__ == "__main__":
	flashOffset = 0
	assetsDir = sys.argv[1] + "/"
	inputFileName = sys.argv[2]
	outputBinFileName = sys.argv[3]
	outputHFileName = sys.argv[4]

	# open input file (list of files, <binfile>, <txtfile>, <pack>
	with open(inputFileName, 'r') as inputFile, open(outputBinFileName, 'wb') as outputBinFile, open(outputHFileName, 'w') as outputHFile:
		outputHFile.write("#ifndef __FLASHOFFSETS_H\n#define __FLASHOFFSETS_H\n\n")
		outputHFile.write("#include \"ArduboyFX.h\"\n\n")

		while True:
			#readline ifile
			line = inputFile.readline()
			if not line:
				break
			if not line.rstrip('\n'):
				break

			# if line starts with #, then it is a comment
			if line.strip()[0] == '#':
				continue

			binFileName, txtFileName, options = line.split(',')
			binFileName = binFileName.strip()
			txtFileName = txtFileName.strip()
			options = options.split()

			# check if the file exists
			fileSize = None
			try:
				fileSize = getsize(assetsDir + binFileName)
			except OSError as oe:
				pass

			if fileSize:
				# write comments if available
				if txtFileName:
					with open(assetsDir + txtFileName, 'r') as txtFile:
						outputHFile.write(txtFile.read())

				# write current flash offset
				writeOffset(outputHFile, binFileName, flashOffset)

				with open(assetsDir + binFileName, 'rb') as binFile:
					# TODO offset feature needed?

					# write binary content to file
					binData = binFile.read()
					flashOffset += len(binData)
					outputBinFile.write(binData)
					# with pad option, pad the file to multiple of pageSize
					flashOffset += padOrAlign(outputBinFile, options, "pad", fileSize, pageSize)
			else:
				if "constexpr_u8" in options:
					# write constexpr value
					writeConstexpr_u8(outputHFile, binFileName, int(txtFileName, 0))
				elif "constexpr_u16" in options:
					# write constexpr value
					writeConstexpr_u16(outputHFile, binFileName, int(txtFileName, 0))
				else:
					# create an aligned offset
					flashOffset += padOrAlign(outputBinFile, options, "pad", flashOffset, pageSize)

					# write current flash offset
					writeOffset(outputHFile, binFileName, flashOffset)


		outputHFile.write("\n#endif")
