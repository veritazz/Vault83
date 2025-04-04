#!/usr/bin/python

import glob
import sys
import struct
from collections import OrderedDict

# size of a block in pixels
blockSize = 64
# size of a half block in pixels
halfBlockSize = int(blockSize / 2)
# total map width in nr of blocks
mapWidth = 32
# total map height in nr of blocks
mapHeight = 32

# The maximum number of triggers allowed in every level
maxTriggerCount = 7
# The maximum number of moving walls in every level
maxMovingWallCount = 10
# The maximum number of doors in every level
maxDoorCount = 5
# The maximum number of sprites in every level (dynamic)
maxSpriteCount = 40
# The maximum number of projectiles in every level (e.g. from shooting walls)
maxProjectileCount = 5
# The maximum number of quests in every level
maxQuestCount = 16
# The maximum number of static sprites in every level
maxStaticSpriteCount = 100
# default filename for binary level data
outputfilename = "leveldata"
# initial players x and y position on the map (x/y are block coordinates)
playerX = None
playerY = None


triggerCount = 0              # number of triggers found in the table
trackedTriggerCount = 0       # number of triggers found in the map
doorCount = 0                 # number of doors found in the table
trackedDoorCount = 0          # number of doors found in the map
movingWallTrack = 0           # temporary flag to indicate if a wall is tracked
movingWallCount = 0           # number of moving walls found in the table
trackedMovingWallCount = 0    # number of moving walls found in the map
movingWallID = 0              # temporary counter for horizontal/vertical moving walls
totalSpriteCount = 0          # number of all sprites found on the map
totalSpriteTableEntries = 0   # number of sprites found in the table
trackedSpriteCount = 0        # number of dynamic sprites found on the map
trackedStaticSpriteCount = 0  # number of static sprites found on the map
specialWallCount = 0          # number of special walls found in the table
trackedSpecialWallCount = 0   # number of special walls found on the map

level = 0                     # currently processed level map



# list to hold the map data parsed from the level file
mapData = []
# dictionaries to hold current vertical/horizontal moving walls
vMovingWall = {}              # temporary dictionary for current vertical moving wall
hMovingWall = {}              # temporary dictionary for current horizontal moving wall
movingWalls = OrderedDict()   # holds all moving walls of this level
# dictionary for non static sprites
mapSprites = OrderedDict()
# dictionary for static sprites
mapStaticSprites = OrderedDict()
# dictionary for special walls
mapSpecialWalls = OrderedDict()

#
# translation table for map blocks
#
mtranslation = {
	"": 0, #"F0",

	"##": 1, #"W0",
	"#1": 2, #"W1",
	"#2": 3, #"W2",
	"#3": 4, #"W3",
	"#4": 5, #"W4",
	"#5": 6, #"W5",
	"#6": 7, #"W6",
	"#7": 8, #"W7",
	"#8": 9, #"W8",
	"#9": 10, #"W9",
	"#a": 11, #"W10",
	"#b": 12, #"W11",
	"#c": 13, #"W12",
	"#d": 14, #"W13",
	"#e": 15, #"W14",

	"#f": 0x10, #"W15",
	"#g": 0x11, #"W16",
	"#h": 0x12, #"W17",
	"#i": 0x13, #"W18",
	"#j": 0x14, #"W19",
	"#k": 0x15, #"W20",
	"#l": 0x16, #"W21",
	"#m": 0x17, #"W22",
	"#n": 0x18, #"W23",
	"#o": 0x19, #"W24",
	"#p": 0x1a, #"W25",
	"#q": 0x1b, #"W26",
	"#r": 0x1c, #"W27",
	"#s": 0x1d, #"W28",
	"#t": 0x1e, #"W29",
	"#u": 0x1f, #"W30",

	"~#": 0x21, #"w0",
	"~1": 0x22, #"w1",
	"~2": 0x23, #"w2",
	"~3": 0x24, #"w3",
	"~4": 0x25, #"w4",
	"~5": 0x26, #"w5",
	"~6": 0x27, #"w6",
	"~7": 0x28, #"w7",
	"~8": 0x29, #"w8",
	"~9": 0x2a, #"w9",
	"~a": 0x2b, #"w10",
	"~b": 0x2c, #"w11",
	"~c": 0x2d, #"w12",
	"~d": 0x2e, #"w13",
	"~e": 0x2f, #"w14",

	"~f": 0x30, #"w15",
	"~g": 0x31, #"w16",
	"~h": 0x32, #"w17",
	"~i": 0x33, #"w18",
	"~j": 0x34, #"w19",
	"~k": 0x35, #"w20",
	"~l": 0x36, #"w21",
	"~m": 0x37, #"w22",
	"~n": 0x38, #"w23",
	"~o": 0x39, #"w24",
	"~p": 0x3a, #"w25",
	"~q": 0x3b, #"w26",
	"~r": 0x3c, #"w27",
	"~s": 0x3d, #"w28",
	"~t": 0x3e, #"w29",

	# TODO no need
	# special walls (S = full block, s = half block)
	"S_": 0, #"F0",
	"S#": 1, #"W0",
	"S1": 2, #"W1",
	"S2": 3, #"W2",
	"S3": 4, #"W3",
	"S4": 5, #"W4",
	"S5": 6, #"W5",
	"S6": 7, #"W6",
	"S7": 8, #"W7",
	"S8": 9, #"W8",
	"S9": 10, #"W9",
	"Sa": 11, #"W10",
	"Sb": 12, #"W11",
	"Sc": 13, #"W12",
	"Sd": 14, #"W13",
	"Se": 15, #"W14",

	"Sf": 0x10, #"W15",
	"Sg": 0x11, #"W16",
	"Sh": 0x12, #"W17",
	"Si": 0x13, #"W18",
	"Sj": 0x14, #"W19",
	"Sk": 0x15, #"W20",
	"Sl": 0x16, #"W21",
	"Sm": 0x17, #"W22",
	"Sn": 0x18, #"W23",
	"So": 0x19, #"W24",
	"Sp": 0x1a, #"W25",
	"Sq": 0x1b, #"W26",
	"Sr": 0x1c, #"W27",
	"Ss": 0x1d, #"W28",
	"St": 0x1e, #"W29",
	"Su": 0x1f, #"W30",

	"s#": 0x21, #"w0",
	"s1": 0x22, #"w1",
	"s2": 0x23, #"w2",
	"s3": 0x24, #"w3",
	"s4": 0x25, #"w4",
	"s5": 0x26, #"w5",
	"s6": 0x27, #"w6",
	"s7": 0x28, #"w7",
	"s8": 0x29, #"w8",
	"s9": 0x2a, #"w9",
	"sa": 0x2b, #"w10",
	"sb": 0x2c, #"w11",
	"sc": 0x2d, #"w12",
	"sd": 0x2e, #"w13",
	"se": 0x2f, #"w14",

	"sf": 0x30, #"w15",
	"sg": 0x31, #"w16",
	"sh": 0x32, #"w17",
	"si": 0x33, #"w18",
	"sj": 0x34, #"w19",
	"sk": 0x35, #"w20",
	"sl": 0x36, #"w21",
	"sm": 0x37, #"w22",
	"sn": 0x38, #"w23",
	"so": 0x39, #"w24",
	"sp": 0x3a, #"w25",
	"sq": 0x3b, #"w26",
	"sr": 0x3c, #"w27",
	"ss": 0x3d, #"w28",
	"st": 0x3e, #"w29",

	# other
	"T": 0x40, #"TRIGGER",
	"D": 0x3f, #"H_DOOR",
	"VS": 0x47, #"V_M_W",
	"v": 0x47, #"V_M_W",
	"VE": 0x47, #"V_M_W",
	"HS": 0x47, #"V_M_W",
	"HE": 0x47, #"V_M_W",
	"P": 0, #"F0",
	"S": 0, #"F0",
	"E": 0, #"F0",
	"I": 0, #"F0",
	"FT": 0x46, #"FLOOR_TRIGGER",
}

def fail(msg):
	print("Error (level {}): ".format(level) + msg)
	sys.exit(1)

def usage():
	print("...")

def save_binary_data(f_data, f, padding=True):

	# pad to next 256byte boundary?
	size = len(f_data)
	for m in range(size):
		mapValue = 0
		mapX = m % mapWidth
		mapY = int(m / mapWidth)
		if f_data[m] == "S" or f_data[m] == "I":
			# find static sprite and modify the map value to be 0x80 + <static sprite id>
			for s in mapStaticSprites.values():
				if s["map-x"] == mapX and s["map-y"] == mapY:
					mapValue = 0x80 + s["id"]
		else:
			mapValue = mtranslation[f_data[m]]

		f.write(struct.pack('<B', mapValue))

	pad = 256 - (size % 256)
	if pad != 256 and padding:
		f.write(struct.pack('<B', 0) * pad)
	else:
		pad = 0

	return size + pad

def readLine(mdatafile):
	mdata = []
	line = mdatafile.readline().rstrip('\n')
	if len(line) == 0:
		return None
	_line = [s for s in line if s != '|']
	#print(_line)
	#print(len(_line))
	for i in range(0, len(_line), 2):
		s = _line[i] + _line[i+1]
		mdata += [s]
	return mdata

def generate_players_position(tag, x, y):
	global playerX
	global playerY
	playerX = x * blockSize + halfBlockSize
	playerY = y * blockSize + halfBlockSize


def trackTrigger(tag, x, y):
	global trackedTriggerCount

	# count the number of triggers
	trackedTriggerCount += 1

def trackDoor(tag, x, y):
	global mapDoors
	global trackedDoorCount

	trackedDoorCount += 1
	#
	# nothing to be done here except for checking if the door in the map
	# also has been flags specified
	#
	for dk in list(mapDoors.keys()):
		door = mapDoors[dk]
		if door["map-x"] == x and door["map-y"] == y:
			return

# called for VS and VE
def trackMovingWall(tag, x, y):
	global vMovingWall
	global movingWalls
	global movingWallID
	global triggerCount
	global trackedMovingWallCount

	if tag == "VS":
		vMovingWall["map-x"] = x
		vMovingWall["map-y"] = y
		vMovingWall["id"] = "V%u" % (movingWallID)

		#
		# create a trigger if this is a pushwall. This trigger will be used to
		# start the activity of the moving wall
		#
		objId = movingWalls[vMovingWall["id"]][2]
		if objId != None:
			createTrigger("T%d %u %u OFF ONESHOT WALL %d 0" % (x, y, triggerCount, objId))
			trackTrigger(tag, x, y)

	global movingWallTrack

	if movingWallTrack == 1:
		# end a track
		movingWallTrack = 0
		movingWallID += 1
		vMovingWall["max-y"] = y
		vMovingWall["flags"] = movingWalls[vMovingWall["id"]][0]
		# if start position is below end position, y must start decrementing
		if vMovingWall["map-y"] > vMovingWall["min-y"]:
			vMovingWall["flags"] &= ~(1 << 0) #"MW_DIRECTION_DEC | "
		vMovingWall["blockid"] = int(movingWalls[vMovingWall["id"]][1], 0)

		# validate if tracked moving wall matches the paramters from the table
		if movingWalls[vMovingWall["id"]][3] != vMovingWall["map-x"]:
			fail("Moving Wall " + vMovingWall["id"] + " has an inconsistent x coordinate")
		if movingWalls[vMovingWall["id"]][4] != vMovingWall["map-y"]:
			fail("Moving Wall " + vMovingWall["id"] + " has an inconsistent y coordinate")
		if movingWalls[vMovingWall["id"]][5] != vMovingWall["max-y"] and \
		   movingWalls[vMovingWall["id"]][5] != vMovingWall["min-y"]:
			fail("Moving Wall " + vMovingWall["id"] + " has an inconsistent end coordinate")

		# add v track in mapData between min-y and max-y
		#print("vMovingWall")
		for y in range(vMovingWall["min-y"]+1, vMovingWall["max-y"]):
			#print("x {} y {}".format(vMovingWall["map-x"], y))
			mapData[vMovingWall["map-x"] + (y * mapWidth)] = "v"

		movingWalls[vMovingWall["id"]] = dict(vMovingWall)
		trackedMovingWallCount += 1
	else:
		# start a track
		vMovingWall["min-y"] = y
		movingWallTrack = 1

# called for HS and HE
def trackHorizontalMovingWall(tag, x, y):
	global hMovingWall
	global movingWalls
	global movingWallID
	global triggerCount
	global trackedMovingWallCount

	if tag == "HS":
		hMovingWall["map-x"] = x
		hMovingWall["map-y"] = y
		hMovingWall["id"] = "H%u" % (movingWallID)

		# create a trigger if this is a pushwall
		objId = movingWalls[hMovingWall["id"]][2]
		if objId != None:
			createTrigger("T%d %u %u OFF ONESHOT WALL %d 0" % (x, y, triggerCount, objId))
			trackTrigger(tag, x, y)

	global movingWallTrack

	if movingWallTrack == 1:
		# end a track
		movingWallTrack = 0
		movingWallID += 1
		hMovingWall["max-y"] = x
		hMovingWall["flags"] = movingWalls[hMovingWall["id"]][0]

		# if start position is after end position, x must start decrementing
		if hMovingWall["map-x"] > hMovingWall["min-y"]:
			hMovingWall["flags"] &= ~(1 << 0) #"MW_DIRECTION_DEC | "

		# add v track in mapData between min-y and max-y (which is x for hMovingWalls)
		#print("hMovingWall")
		for x in range(hMovingWall["min-y"]+1, hMovingWall["max-y"]):
			#print("x {} y {}".format(x, hMovingWall["map-y"]))
			mapData[x + (hMovingWall["map-y"] * mapWidth)] = "v"

		hMovingWall["blockid"] = int(movingWalls[hMovingWall["id"]][1], 0)
		movingWalls[hMovingWall["id"]] = dict(hMovingWall)
		trackedMovingWallCount += 1
	else:
		# start a track
		hMovingWall["min-y"] = x
		movingWallTrack = 1

def trackStaticSprite(tag, x, y):
	global mapStaticSprites
	global totalSpriteCount
	global trackedStaticSpriteCount

	spriteName = "S%u" % totalSpriteCount
	totalSpriteCount += 1

	s = mapStaticSprites[spriteName]

	if s["map-x"] != x:
		fail("x coordinate of sprite " + spriteName + " does not match ({}!={})".format(x, s["map-x"]))
	if s["map-y"] != y:
		fail("y coordinate of sprite " + spriteName + " does not match")

	s["id"] = trackedStaticSpriteCount

	trackedStaticSpriteCount += 1

def trackSprite(tag, x, y):
	global mapSprites
	global trackedSpriteCount
	global totalSpriteCount

	spriteName = "S%u" % totalSpriteCount
	totalSpriteCount += 1

	s = mapSprites[spriteName]

	if s["map-x"] != x:
		fail("x coordinate of sprite " + spriteName + " does not match")
	if s["map-y"] != y:
		fail("y coordinate of sprite " + spriteName + " does not match")

	s["map-x"] = x * blockSize + halfBlockSize
	s["map-y"] = y * blockSize + halfBlockSize

	trackedSpriteCount += 1

#
# track special walls
#
def trackSpecialWall(tag, x, y):
	global mapSpecialWalls
	global specialWallCount

	specialWallName = "W%u" % specialWallCount
	specialWallCount += 1

	s = mapSpecialWalls[specialWallName]
	s["map-x"] = x
	s["map-y"] = y
	mapSpecialWalls[specialWallName] = s

# functions that are executed during vertical scan of the map
vertical_special_fn = {
	"P": generate_players_position,
	"D": trackDoor,
	"T": trackTrigger,
	"FT": trackTrigger,
	"VS": trackMovingWall,
	"VE": trackMovingWall,
	"S": trackStaticSprite,
	"E": trackSprite, # just for convenience
	"I": trackStaticSprite, # just for convenience

	# TODO no need
	"S_": trackSpecialWall,
	"S#": trackSpecialWall,
	"S1": trackSpecialWall,
	"S2": trackSpecialWall,
	"S3": trackSpecialWall,
	"S4": trackSpecialWall,
	"S5": trackSpecialWall,
	"S6": trackSpecialWall,
	"S7": trackSpecialWall,
	"S8": trackSpecialWall,
	"S9": trackSpecialWall,
	"Sa": trackSpecialWall,
	"Sb": trackSpecialWall,
	"Sc": trackSpecialWall,
	"Sd": trackSpecialWall,
	"Se": trackSpecialWall,
	"Sf": trackSpecialWall,
	"Sg": trackSpecialWall,
	"Sh": trackSpecialWall,
	"Si": trackSpecialWall,
	"Sj": trackSpecialWall,
	"Sk": trackSpecialWall,
	"Sl": trackSpecialWall,
	"Sm": trackSpecialWall,
	"Sn": trackSpecialWall,
	"So": trackSpecialWall,
	"Sp": trackSpecialWall,
	"Sq": trackSpecialWall,
	"Sr": trackSpecialWall,
	"Ss": trackSpecialWall,
	"St": trackSpecialWall,
	"Su": trackSpecialWall,
	"s#": trackSpecialWall,
	"s1": trackSpecialWall,
	"s2": trackSpecialWall,
	"s3": trackSpecialWall,
	"s4": trackSpecialWall,
	"s5": trackSpecialWall,
	"s6": trackSpecialWall,
	"s7": trackSpecialWall,
	"s8": trackSpecialWall,
	"s9": trackSpecialWall,
	"sa": trackSpecialWall,
	"sb": trackSpecialWall,
	"sc": trackSpecialWall,
	"sd": trackSpecialWall,
	"se": trackSpecialWall,
	"sf": trackSpecialWall,
	"sg": trackSpecialWall,
	"sh": trackSpecialWall,
	"si": trackSpecialWall,
	"sj": trackSpecialWall,
	"sk": trackSpecialWall,
	"sl": trackSpecialWall,
	"sm": trackSpecialWall,
	"sn": trackSpecialWall,
	"so": trackSpecialWall,
	"sp": trackSpecialWall,
	"sq": trackSpecialWall,
	"sr": trackSpecialWall,
	"ss": trackSpecialWall,
	"st": trackSpecialWall,
}

# functions that are executed during horizontal scan of the map
horizontal_special_fn = {
	"HS": trackHorizontalMovingWall,
	"HE": trackHorizontalMovingWall,
}

def print_map_tile_comment(f_data, f):
	f.write("\t/*\n\t *");
	for b in range(len(f_data)):
		f.write("%s" % f_data[b])
		if (b + 1) % 4 == 0:
			f.write("\n\t *")
		else:
			f.write("|");
	f.write("/\n")

def print_map_tile_data(f_data, f):
	f.write("\t")
	for b in range(len(f_data)):
		f.write("%-12.12s," % f_data[b])
		if (b + 1) % 4 == 0 and ((b + 1) != len(f_data)):
			f.write("\n\t")
	f.write("\n")

mapTriggers = OrderedDict()

triggerStates = {
	"on"          : 1 << 0, #"TRIGGER_STATE_ON      ",
	"off"         : 0 << 0, #"TRIGGER_STATE_OFF     ",
	"oneshot"     : 0 << 1, #"TRIGGER_TYPE_ONE_SHOT ",
	"switch"      : 1 << 1, #"TRIGGER_TYPE_SWITCH   ",
	"touch"       : 2 << 1, #"TRIGGER_TYPE_TOUCH    ",
	"floor"       : 3 << 1, #"TRIGGER_TYPE_FLOOR    ",
	"door"        : 0 << 3, #"TRIGGER_OBJ_DOOR      ",
	"wall"        : 1 << 3, #"TRIGGER_OBJ_VMW       ",
	"dialog"      : 2 << 3, #"TRIGGER_OBJ_DIALOG    ",
	"next_level"  : 3 << 3, #"TRIGGER_OBJ_NEXT_LEVEL",
	"quest"       : 4 << 3, #"TRIGGER_OBJ_QUEST     ",
	"menu"        : 5 << 3, #"TRIGGER_OBJ_MENU      ",
}

def createTrigger(line):
	global triggerCount
	global mapTriggers

	triggerCount += 1
	#print("createTrigger")

	#  0     1   2     3        4             5           6        7
	# T<id> <x> <y> <state>   <type>      <obj type>   <obj id> <blockId>
	parts = line.split()
	flags  = triggerStates[parts[3].lower()]
	flags += triggerStates[parts[4].lower()]
	flags += triggerStates[parts[5].lower()]

	mapTriggers[parts[0]] = dict({"map-x": int(parts[1], 0),
				      "map-y": int(parts[2], 0),
				      "flags": flags,
				      "obj-id": int(parts[6], 0),
				      "blockid": int(parts[7], 0),
				      "order": triggerStates[parts[4].lower()]})

mapDoors = OrderedDict()

def isYes(txt):
	if txt.lower() == "yes":
		return True
	return False

def isOn(txt):
	if txt.lower() == "on":
		return True
	return False

def createMap(line):
	global mapData

	mdata = [b.strip() for b in line.split('|')]

	# strip the first column
	mapData += mdata[1:]

def createDoor(line):
	global doorCount
	global mapDoors

	doorCount += 1

	#print("createDoor")
	#print(line)

	#         0    1   2      4        5        6        7
	# Doors D<id> <x> <y> <locked> <trigger> <flaky> <blockId>
	did, x, y, locked, trigger, flaky, blockid = line.split()

	# create the flags for the engine
	flags = int(blockid, 0) << 4
	if isYes(locked):
		flags += 1 << 1 #" | DOOR_FLAG_LOCKED "
	if isYes(trigger):
		flags += 1 << 2 #" | DOOR_FLAG_TRIGGER"
	if isYes(flaky):
		flags += 1 << 0 #" | DOOR_FLAG_FLAKY  "

	door = {}
	door["map-x"] = int(x, 0)
	door["map-y"] = int(y, 0)
	door["flags"] = flags
	mapDoors[did] = door

#
# collect all paramters in a list first, the corresponding track function will later
# replace it with a properly filled dictionary
#
def createMovingWall(line):
	global movingWalls
	global movingWallCount

	movingWallCount += 1

	#     0    1   2    3      4        5         6        7        8         9
	# V/H<id> <x> <y> <end> <damage> <active> <oneshot> <speed> <blockId> <pushwall>
	parts = line.split()
	wallID = parts[0]

	x = int(parts[1], 0)
	y = int(parts[2], 0)
	end = int(parts[3], 0)

	# assume start position is above end position, so y must increment
	flags = 1 << 0 #"MW_DIRECTION_INC | "
	if isYes(parts[4]):
		flags += 1 << 5 #"VMW_FLAG_DAMAGE | "
	if isYes(parts[5]):
		flags += 1 << 6 #"VMW_FLAG_ACTIVE | "
	if isYes(parts[6]):
		flags += 1 << 7 #"VMW_FLAG_ONESHOT | "
	# set speed
	flags += (int(parts[7]) & 0x3) << 1 # + " << 1"
	# set type
	if wallID.startswith("H"):
		flags += 1 << 3   # set type flag to 1 if horizontal wall

	# create a trigger if this is a push wall
	if isYes(parts[9]):
		# one shot trigger that activates the wall
		obj_id = movingWallCount
		movingWalls[wallID] = [flags, parts[8], obj_id, x, y, end]
	else:
		movingWalls[wallID] = [flags, parts[8], None, x, y, end]


def createSprite(line):
	global mapSprites
	global mapStaticSprites
	global totalSpriteTableEntries

	#print("createSprite")

	parts = line.split()
	#           0     1   2     3       4       5        6
	# Sprites (S<id> <x> <y> <family> <type> <health> <flags>)
	spriteName = parts[0]
	x = int(parts[1], 0)
	y = int(parts[2], 0)
	spriteFamily = parts[3]
	spriteType = int(parts[4], 0)
	spriteHealth = int(parts[5], 0)
	spriteFlags = int(parts[6])

	sprite = {}
	sprite["map-x"] = x
	sprite["map-y"] = y
	sprite["name"] = spriteName
	sprite["type"] = spriteType
	sprite["health"] = spriteHealth
	spriteID = "S%u" % totalSpriteTableEntries
	totalSpriteTableEntries += 1

	if spriteFamily == 'E':
		mapSprites[spriteID] = sprite
	else:
		mapStaticSprites[spriteID] = sprite

mapQuests = OrderedDict()
questCount = 0

def createQuest(line):
	global mapQuests
	global questCount
	parts = line.split()
	questID = parts[0]

	#print("createQuest")

	# get enemy type kill counters
	enemyTypeKills = []
	for et in range(4):
		enemyTypeKills.append(int(parts[et + 1]))

	# get sprite type kill counters
	spriteTypeKills = []
	for st in range(2):
		spriteTypeKills.append(int(parts[st + 5]))

	# get item type counters
	itemType = []
	for it in range(4):
		itemType.append(int(parts[it + 7]))

	# get event type counters
	eventType = []
	for et in range(8):
		eventType.append(int(parts[et + 11]))

	# Quests (enemy type counters, sprite type counters, item type counters, event type counters)
	#Q0             0 0 0 0                  0 0              0 0 0 0           0 0 0 0 0 0 0 0
	mapQuests[questID] = {
		"enemyTypeKills": enemyTypeKills,
		"spriteTypeKills": spriteTypeKills,
		"itemType": itemType,
		"eventType": eventType
		}

	questCount += 1

specialWallViewAngleFlags = {
	"shoot_up"          : 3 << 1,
	"shoot_down"        : 1 << 1,
	"shoot_left"        : 2 << 1,
	"shoot_right"       : 0 << 1,
}

def createSpecialWall(line):
	global mapSpecialWalls
	global trackedSpecialWallCount

	#print("createSpecialWall")
	trackedSpecialWallCount += 1

	#  0     1   2      3             4
	# W<id> <x> <y> <direction> <projectile id>
	wid, x, y, direction, pid = line.split()

	s = {}
	# set x/y coordinate
	s["map-x"] = x
	s["map-y"] = y
	# add shooting direction to flags
	s["flags"] = specialWallViewAngleFlags[direction.lower()]
	# add projectile id to flags
	s["flags"] += (int(pid) & 0xf) << 3
	# set projectile flag in flags
	s["flags"] += 0x80

	mapSpecialWalls[wid] = s

def handleTag(line, mdatafile, tag, handler):
	if tag in line:
		while True:
			#line = readLine(mdatafile)
			line = mdatafile.readline()
			#print(line)
			if not line.rstrip('\n'):
				break;
			# in case the tag has no entries
			if "none" in line:
				break
			if line.startswith('#'):
				continue
			handler(line);

if __name__ == "__main__":
	with open(outputfilename + ".h", 'w') as hfile:

		hfile.write("#ifndef __LEVELDATA_H\n")
		hfile.write("#define __LEVELDATA_H\n\n")
		hfile.write("#define MAP_WIDTH            %u\n" % mapWidth)
		hfile.write("#define MAP_HEIGHT           %u\n" % mapHeight)
		hfile.write("#define MAX_MOVING_WALLS     %u\n" % maxMovingWallCount)
		hfile.write("#define MAX_DOORS            %u\n" % maxDoorCount)
		hfile.write("#define MAX_TRIGGERS         %u\n" % maxTriggerCount)
		hfile.write("#define MAX_SPRITES          %u /* non static sprites */\n" % maxSpriteCount)
		hfile.write("#define MAX_PROJECTILES      %u /* projectiles */\n" % maxProjectileCount)
		hfile.write("#define TOTAL_SPRITES        %u /* non static sprites + projectiles */\n" % (maxSpriteCount + maxProjectileCount))
		hfile.write("#define MAX_QUESTS           %u\n" % maxQuestCount)
		hfile.write("#define MAX_STATIC_SPRITES   %u\n" % maxStaticSpriteCount)

		total_size = 0
		artifacts_dir = sys.argv[1] + "/"
		output_dir = sys.argv[2] + "/"

		for map_filename in sorted(glob.glob(artifacts_dir + "map*.txt")):
			level += 1
			print(".. processing " + map_filename)

			# reset all counters and dictionaries for each level
			playerX = None
			playerY = None
			mapData = []
			triggerCount = 0
			trackedTriggerCount = 0
			doorCount = 0
			trackedDoorCount = 0
			movingWallCount = 0
			trackedMovingWallCount = 0
			movingWallTrack = 0
			movingWallID = 0
			vMovingWall = {}
			hMovingWall = {}
			movingWalls = OrderedDict()
			mapSprites = OrderedDict()
			mapStaticSprites = OrderedDict()
			totalSpriteTableEntries = 0
			totalSpriteCount = 0
			trackedSpriteCount = 0
			trackedStaticSpriteCount = 0
			mapTriggers = OrderedDict()
			mapDoors = OrderedDict()
			mapQuests = OrderedDict()
			questCount = 0
			mapSpecialWalls = OrderedDict()
			specialWallCount = 0
			trackedSpecialWallCount = 0

			with open(map_filename) as mdatafile, \
			     open(output_dir + "level%u" % level + ".bin", 'wb') as bfile, \
			     open(output_dir + "level%u" % level + ".txt", 'w') as tfile, \
			     open(output_dir + "level%u_quests" % level + ".bin", "wb") as qbfile, \
			     open(output_dir + "level%u_specialWalls" % level + ".bin", "wb") as swfile:
				#
				# parse level file
				#
				while True:
					#line = readLine(mdatafile)
					line = mdatafile.readline()

					# check if end of file
					if line == '':
						break

					line.rstrip('\n')
					#
					# read in the map data
					#
					handleTag(line, mdatafile, "Map", createMap)
					#
					# create doors
					#
					handleTag(line, mdatafile, "Doors", createDoor)
					#
					# create triggers
					#
					handleTag(line, mdatafile, "Triggers", createTrigger)
					#
					# create moving walls
					#
					handleTag(line, mdatafile, "Moving Walls", createMovingWall)
					#
					# create sprites
					#
					handleTag(line, mdatafile, "Sprites", createSprite)
					#
					# create Quests
					#
					handleTag(line, mdatafile, "Quests", createQuest)
					#
					# create special walls
					#
					handleTag(line, mdatafile, "Special", createSpecialWall)


				# walk the whole map columnwise and try to execute any special function
				for mapX in range(mapWidth):
					for mapY in range(mapHeight):
						offset = mapY * mapWidth + mapX
						try:
							fn = vertical_special_fn[mapData[offset]]
							fn(mapData[offset], mapX, mapY)
						except KeyError:
							pass

				# walk the whole map rowwise and try to execute any special function
				movingWallID = 0     # reset this as it is needed for horizontal wall tracking
				for mapY in range(mapHeight):
					for mapX in range(mapWidth):
						offset = mapY * mapWidth + mapX
						try:
							fn = horizontal_special_fn[mapData[offset]]
							fn(mapData[offset], mapX, mapY)
						except KeyError:
							pass


				#==========================================================
				# write binary level data file
				#
				# the layout is as follows:
				#   [mapWidth*mapHeight] map data
				#   [maxTriggerCount] triggers init data
				#   [maxMovingWallCount] moving walls init data
				#   [maxDoorCount] doors init data
				#   [maxSpriteCount+maxProjectileCount] dynamic sprites init data
				#   [maxSpriteCount] sprites health table
				#   [maxStaticSpriteCount] static sprite table (e.g. items)
				#   [] miscellaneous map information
				#   [] players init data
				#

				#----------------------------------------------------------
				# write map data
				save_binary_data(mapData, bfile)

				#----------------------------------------------------------
				# write triggers init data
				#
				if triggerCount != trackedTriggerCount:
					fail("trigger count mismatch between map and table\n")

				if triggerCount > maxTriggerCount:
					fail("too many triggers\n")

				# sort mapTriggers first so floor triggers are at the end
				sortedMapTriggers = dict(sorted(mapTriggers.items(), key=lambda item: item[1]["order"]))

				for tk in list(sortedMapTriggers.keys()):
					t = sortedMapTriggers[tk]
					bfile.write(struct.pack('<B', t["flags"]))
					bfile.write(struct.pack('<B', t["obj-id"]))
					bfile.write(struct.pack('<B', t["map-x"]))
					bfile.write(struct.pack('<B', t["map-y"]))
					bfile.write(struct.pack('<B', t["blockid"]))
					bfile.write('\0'.encode('utf-8'))  # pad for timeout field

				# pad up to max trigger count
				for pad in range(maxTriggerCount - triggerCount):
					bfile.write('\0'.encode('utf-8') * 6)

				#----------------------------------------------------------
				# write moving walls init data
				#
				if movingWallCount != trackedMovingWallCount:
					fail("moving wall count mismatch between map and table\n")

				if movingWallCount > maxMovingWallCount:
					fail("too many moving walls\n")

				for mwk in list(movingWalls.keys()):
					mwd = movingWalls[mwk]
					bfile.write(struct.pack('<B', mwd["flags"]))
					bfile.write(struct.pack('<B', mwd["map-x"]))
					bfile.write(struct.pack('<B', mwd["map-y"]))
					bfile.write(struct.pack('<B', mwd["min-y"]))
					bfile.write(struct.pack('<B', mwd["max-y"]))
					bfile.write(struct.pack('<B', mwd["blockid"]))
					bfile.write('\0'.encode('utf-8'))  # pad


				# pad up to max moving wall count
				for pad in range(maxMovingWallCount - movingWallCount):
					bfile.write('\0'.encode('utf-8') * 7)

				#----------------------------------------------------------
				# write doors init data
				#
				if doorCount != trackedDoorCount:
					fail("door count mismatch between map and table\n")

				if doorCount > maxDoorCount:
					fail("too many doors\n")

				for dk in list(mapDoors.keys()):
					door = mapDoors[dk]
					bfile.write(struct.pack('<B', door["flags"]))
					bfile.write(struct.pack('<B', door["map-x"]))
					bfile.write(struct.pack('<B', door["map-y"]))
					bfile.write('\0'.encode('utf-8') * 3)  # pad

				# pad up to max door count
				for pad in range(maxDoorCount - doorCount):
					bfile.write('\0'.encode('utf-8') * 6)

				#----------------------------------------------------------
				# write dynamic sprites init data
				#
				if trackedSpriteCount > maxSpriteCount:
					fail("too many sprites (%u/%u)\n" % (trackedSpriteCount, maxSpriteCount))

				for spr in list(mapSprites.keys()):
					s = mapSprites[spr]
					mapX = s["map-x"]
					mapY = s["map-y"]
					# pack x and y into a 24bit value
					mapXY = (mapY << 12) | mapX
					bfile.write(struct.pack('<B', mapXY & 0xff))
					bfile.write(struct.pack('<B', (mapXY >> 8) & 0xff))
					bfile.write(struct.pack('<B', (mapXY >> 16) & 0xff))

					# compose flags
					flags = 0
					flags |= (s["type"] & 0x3) << 3
					bfile.write(struct.pack('<B', flags))

				# pad up to max sprite count
				for pad in range(maxSpriteCount + maxProjectileCount - trackedSpriteCount):
					bfile.write(struct.pack('<BBB', 0, 0, 0))
					bfile.write(struct.pack('<B', 1))  # flags: inactive

				#----------------------------------------------------------
				# write dynamic sprites health table
				#
				for spr in list(mapSprites.keys()):
					s = mapSprites[spr]
					bfile.write(struct.pack('<B', s["health"]))

				# pad up to max sprite count
				for pad in range(maxSpriteCount - trackedSpriteCount):
					bfile.write(struct.pack('<B', 0))


				#----------------------------------------------------------
				# write static sprites init data
				#
				if trackedStaticSpriteCount > maxStaticSpriteCount:
					fail("too many static sprites (%u/%u)\n" % (trackedStaticSpriteCount, maxStaticSpriteCount))

				for spr in list(mapStaticSprites.keys()):
					s = mapStaticSprites[spr]
					_id = s["id"]
					# compose flags
					flags = 1                  # active
					# if health is set, sprite is destroyable
					if s["health"] == 0:
						flags |= 0x80      # non-shootable
					flags |= s["type"] << 1    # type
					bfile.write(struct.pack('<B', flags))

				# pad up to max sprite count
				for pad in range(maxStaticSpriteCount - trackedStaticSpriteCount):
					bfile.write(struct.pack('<B', 0))

				#----------------------------------------------------------
				# miscellaneous map information
				#
				bfile.write(struct.pack('<B', trackedSpriteCount))      # save number of sprites
				bfile.write(struct.pack('<B', trackedDoorCount))        # save number of doors
				bfile.write(struct.pack('<B', trackedMovingWallCount))  # save number of moving walls
				bfile.write(struct.pack('<B', trackedTriggerCount))     # save number of triggers
				bfile.write(struct.pack('<B', questCount))              # save number of quest
				bfile.write(struct.pack('<B', trackedSpecialWallCount)) # save number of special walls

				#----------------------------------------------------------
				# write players init data
				#
				if playerX == None or playerY == None:
					fail("no player position defined\n");

				bfile.write(struct.pack('<H', playerX))
				bfile.write(struct.pack('<H', playerY))
				bfile.write(struct.pack('<B', int(playerX / blockSize)))
				bfile.write(struct.pack('<B', int(playerY / blockSize)))
				bfile.write(struct.pack('<h', 0))

				# TODO player health, ammo and the like?

				#==========================================================
				#
				# TODO create quests binfile
				#
				for q in list(mapQuests.keys()):
					s = mapQuests[q]
					qbfile.write(struct.pack('<BBBB',     *s["enemyTypeKills"]))
					qbfile.write(struct.pack('<BB',       *s["spriteTypeKills"]))
					qbfile.write(struct.pack('<BBBB',     *s["itemType"]))
					qbfile.write(struct.pack('<BBBBBBBB', *s["eventType"]))

				if questCount > maxQuestCount:
					print(("Error: Map " + map_filename + " has too many quests\n"))
					sys.exit(1)

				#==========================================================
				#
				# write special wall data
				#
				if specialWallCount != trackedSpecialWallCount:
					fail("special wall count mismatch between map and table\n")

				for sw in list(mapSpecialWalls.keys()):
					s = mapSpecialWalls[sw]
					swfile.write(struct.pack('<B', s["map-x"]))
					swfile.write(struct.pack('<B', s["map-y"]))
					swfile.write(struct.pack('<B', s["flags"]))

				#==========================================================
				#
				# calculate level statistics
				#
				mapSizeBytes = len(mapData)
				mapTriggerSizeBytes = len(mapTriggers) * 6
				movingWallInitDataSizeBytes = len(list(movingWalls.keys())) * 6
				doorsInitDataSizeBytes = len(list(mapDoors.keys())) * 3
				spritesInitDataSizeBytes = len(mapSprites) * 5
				questSizeBytes = len(mapQuests) * 18
				specialWallsSizeBytes = len(mapSpecialWalls) * 3 + 1
				staticSpritesDataSizeBytes = trackedStaticSpriteCount * 1

				sumMapSize = mapSizeBytes
				sumMapSize += mapTriggerSizeBytes
				sumMapSize += movingWallInitDataSizeBytes
				sumMapSize += doorsInitDataSizeBytes
				sumMapSize += spritesInitDataSizeBytes
				sumMapSize += questSizeBytes
				sumMapSize += specialWallsSizeBytes
				sumMapSize += staticSpritesDataSizeBytes

				# TODO count enemies, items

				tfile.write("/*\n")
				tfile.write(" * ==================================================\n")
				tfile.write(" *  Summary for %s\n" % (map_filename))
				tfile.write(" * ==================================================\n")
				tfile.write(" *   triggers                                    : %u\n" % (triggerCount))
				tfile.write(" *   doors                                       : %u\n" % (doorCount))
				tfile.write(" *   moving walls                                : %u\n" % (movingWallCount))
				tfile.write(" *   sprites                                     : %u\n" % (trackedSpriteCount))
				tfile.write(" *   static sprites                              : %u\n" % (trackedStaticSpriteCount))
				tfile.write(" *   quests                                      : %u\n" % (questCount))
				tfile.write(" *   special walls                               : %u\n" % (specialWallCount))
				tfile.write(" * ----------------------------------------------------\n")
				tfile.write(" *   memory used for map                  (bytes): %u\n" % (mapSizeBytes))
				tfile.write(" *   memory used for triggers             (bytes): %u\n" % (mapTriggerSizeBytes))
				tfile.write(" *   memory used for moving walls         (bytes): %u\n" % (movingWallInitDataSizeBytes))
				tfile.write(" *   memory used for doors                (bytes): %u\n" % (doorsInitDataSizeBytes))
				tfile.write(" *   memory used for sprites              (bytes): %u\n" % (spritesInitDataSizeBytes + staticSpritesDataSizeBytes))
				tfile.write(" *   memory used for quests               (bytes): %u\n" % (questSizeBytes))
				tfile.write(" *   memory used for special walls        (bytes): %u\n" % (specialWallsSizeBytes))
				tfile.write(" * ----------------------------------------------------\n")
				tfile.write(" *   sum                                  (bytes): %u\n" % (sumMapSize))
				tfile.write(" * ====================================================\n")
				tfile.write(" */\n")


		# create a define for the number of levels
		hfile.write("#define MAX_LEVELS           %u\n" % level)

		# finalize the .h file
		hfile.write("\n\n")
		hfile.write("#endif")

