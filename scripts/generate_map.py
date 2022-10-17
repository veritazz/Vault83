#!/usr/bin/python

import glob
import sys
import struct
from collections import OrderedDict

blockSize = 64
halfBlockSize = int(blockSize / 2)
mapWidth = 32
mapHeight = 32

maxTriggerCount = 5
maxMovingWallCount = 5
maxDoorCount = 5
maxSpriteCount = 40
maxProjectileCount = 5
maxQuestCount = 16
maxStaticSpriteCount = 100

#
# [4096] map data
# [] triggers init data
# [] moving walls init data
# [] doors init data
# [] sprites init data
# [] players init data
#


#
# translation table for map blocks
#
mtranslation = {
	"  ": 0, #"F0",
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
	"T ": 64, #"TRIGGER",
	"D ": 63, #"H_DOOR",
	"VS": 65, #"V_M_W",
	"v ": 65, #"V_M_W",
	"VE": 65, #"V_M_W",
	"P ": 0, #"F0",
	"S ": 0, #"F0",
	"E ": 0, #"F0",
	"I ": 0, #"F0",
}

def save_binary_data(f_data, f, padding=True):

	# pad to next 256byte boundary?
	size = len(f_data)
	for m in range(size):
		mapValue = 0
		mapX = m % mapWidth
		mapY = int(m / mapWidth)
		if f_data[m] == "S " or f_data[m] == "I ":
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

def usage():
	print("...")

def readLine(mdatafile):
	mdata = []
	line = mdatafile.readline().rstrip('\n')
	if len(line) == 0:
		return None
	_line = [s for s in line if s != '|']
	for i in range(0, len(_line), 2):
		s = _line[i] + _line[i+1]
		mdata += [s]
	return mdata

outputfilename = "leveldata"

mapData = []

playerX = 0
playerY = 0

def generate_players_position(hfile, tag, x, y):
	global playerX
	global playerY
	playerX = x * blockSize + halfBlockSize
	playerY = y * blockSize + halfBlockSize

triggerCount = 0
doorCount = 0
movingWallCount = 0
movingWallTrack = 0
movingWallID = 0
movingWall = {}
movingWalls = OrderedDict()


def trackTrigger(hfile, tag, x, y):
	global triggerCount
	global mapTriggers

	d = mapTriggers["T%u" % (triggerCount)]

	d["x"] = x
	d["y"] = y

	mapTriggers["T%u" % (triggerCount)] = d

	# count the number of triggers
	triggerCount += 1

def trackDoor(hfile, tag, x, y):
	global doorCount
	global mapDoors

	flags = mapDoors["D%u" % (doorCount)]
	mapDoors["D%u" % (doorCount)] = [flags, x, y]

	# count the number of doors
	doorCount += 1

def trackMovingWall(hfile, tag, x, y):
	global movingWall
	global movingWallID

	if tag == "VS":
		global movingWallCount
		# count the wall for statistics
		movingWallCount += 1

		movingWall["map-x"] = x
		movingWall["map-y"] = y
		movingWall["id"] = "V%u" % (movingWallID)

	global movingWallTrack
	global movingWalls

	if movingWallTrack == 1:
		# end a track
		movingWallTrack = 0
		movingWallID += 1
		movingWall["max-y"] = y
		movingWall["flags"] = movingWalls[movingWall["id"]][0]
		# if start position if below end position, y must start decrementing
		if movingWall["map-y"] > movingWall["min-y"]:
			movingWall["flags"] &= ~(1 << 0) #"MW_DIRECTION_DEC | "
		movingWall["blockid"] = int(movingWalls[movingWall["id"]][1], 0)
		movingWalls[movingWall["id"]] = dict(movingWall)
	else:
		# start a track
		movingWall["min-y"] = y
		movingWallTrack = 1

totalSpriteCount = 0
# dictionary for not static sprites
mapSprites = OrderedDict()
spriteCount = 0
# dictionary for static sprites
mapStaticSprites = OrderedDict()
staticSpriteCount = 0

def trackStaticSprite(hfile, tag, x, y):
	global mapStaticSprites
	global totalSpriteCount
	global staticSpriteCount

	spriteName = "S%u" % totalSpriteCount
	totalSpriteCount += 1

	s = mapStaticSprites[spriteName]
	s["map-x"] = x # * blockSize + halfBlockSize
	s["map-y"] = y # * blockSize + halfBlockSize
	s["id"] = staticSpriteCount

	staticSpriteCount += 1

def trackSprite(hfile, tag, x, y):
	global mapSprites
	global spriteCount
	global totalSpriteCount

	spriteName = "S%u" % totalSpriteCount
	totalSpriteCount += 1

	s = mapSprites[spriteName]
	s["map-x"] = x * blockSize + halfBlockSize
	s["map-y"] = y * blockSize + halfBlockSize

	spriteCount += 1

#
# track special walls
#
mapSpecialWalls = OrderedDict()
specialWallCount = 0

def trackSpecialWall(hfile, tag, x, y):
	global mapSpecialWalls
	global specialWallCount

	specialWallName = "W%u" % specialWallCount
	specialWallCount += 1

	s = mapSpecialWalls[specialWallName]
	s["map-x"] = x
	s["map-y"] = y
	mapSpecialWalls[specialWallName] = s

special_fn = {
	"P ": generate_players_position,
	"D ": trackDoor,
	"T ": trackTrigger,
	"VS": trackMovingWall,
	"VE": trackMovingWall,
	"S ": trackStaticSprite,
	"E ": trackSprite, # just for convenience
	"I ": trackStaticSprite, # just for convenience

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
	"door"        : 0 << 3, #"TRIGGER_OBJ_DOOR      ",
	"wall"        : 1 << 3, #"TRIGGER_OBJ_VMW       ",
	"dialog"      : 2 << 3, #"TRIGGER_OBJ_DIALOG    ",
	"next_level"  : 3 << 3, #"TRIGGER_OBJ_NEXT_LEVEL",
	"quest"       : 4 << 3, #"TRIGGER_OBJ_QUEST     ",
}

def createTrigger(line):
	global mapTriggers
	# Triggers (T<id> <state> <type> <obj type> <obj id> <blockId>
	parts = line.split()
	flags  = triggerStates[parts[1].lower()]
	flags += triggerStates[parts[2].lower()]
	flags += triggerStates[parts[3].lower()]

	mapTriggers[parts[0]] = dict({"flags": flags, "obj-id": int(parts[4], 0), "blockid": int(parts[5], 0)})

mapDoors = OrderedDict()

def createDoor(line):
	global mapDoors
	# Doors (D<id> <locked> <trigger> <flaky> <blockId>
	parts = line.split()
	flags = int(parts[4], 0) << 4 #"%#x" % (int(parts[4], 0) << 4)
	if parts[1].lower() == "yes":
		flags += 1 << 1 #" | DOOR_FLAG_LOCKED "
	if parts[2].lower() == "yes":
		flags += 1 << 2 #" | DOOR_FLAG_TRIGGER"
	if parts[3].lower() == "yes":
		flags += 1 << 0 #" | DOOR_FLAG_FLAKY  "

	mapDoors[parts[0]] = flags


def createMovingWall(line):
	global movingWalls
	# Moving Walls (V<id> <damage> <active> <oneshot> <speed> <blockId>)
	parts = line.split()
	wallID = parts[0]
	flags = 1 << 0 #"MW_DIRECTION_INC | "
	if parts[1].lower() == "yes":
		flags += 1 << 5 #"VMW_FLAG_DAMAGE | "
	if parts[2].lower() == "yes":
		flags += 1 << 6 #"VMW_FLAG_ACTIVE | "
	if parts[3].lower() == "yes":
		flags += 1 << 7 #"VMW_FLAG_ONESHOT | "
	flags += int(parts[4]) << 1 # + " << 1"
	movingWalls[wallID] = [flags, parts[5]]

totalSpriteTableEntries = 0

def createSprite(line):
	global mapSprites
	global mapStaticSprites
	global totalSpriteTableEntries

	parts = line.split()
	# Sprites (S<id> <type> <version> <voffset> <health> <flags>)
	spriteName = parts[0]
	spriteFamily = parts[1]
	spriteVersion = int(parts[2], 0)
	spriteVOffset = int(parts[3], 0)
	spriteHealth = int(parts[4], 0)
	spriteFlags = int(parts[5])

	sprite = {}
	sprite["name"] = spriteName
	sprite["type"] = spriteVersion
	sprite["voffset"] = spriteVOffset
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

specialWallFlags = {
	"shoot_up"          : 3 << 1,
	"shoot_down"        : 1 << 1,
	"shoot_left"        : 2 << 1,
	"shoot_right"       : 0 << 1,
}

def createSpecialWall(line):
	global mapSpecialWalls

	parts = line.split()

	s = {}
	# add shooting direction to flags
	s["flags"] = specialWallFlags[parts[1].lower()]
	# add projectile id to flags
	s["flags"] += (int(parts[2]) & 0xf) << 3
	# set projectile flag in flags
	s["flags"] += 0x80

	mapSpecialWalls[parts[0]] = s

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

		level = 0

		for map_filename in sorted(glob.glob(artifacts_dir + "map*.txt")):
			level += 1
			print(".. processing " + map_filename)
			lines = 0

			mapData = []

			playerX = 0
			playerY = 0

			triggerCount = 0
			doorCount = 0
			movingWallCount = 0
			movingWallTrack = 0
			movingWallID = 0
			movingWall = {}
			movingWalls = OrderedDict()
			mapSprites = OrderedDict()
			mapStaticSprites = OrderedDict()
			totalSpriteTableEntries = 0
			totalSpriteCount = 0
			spriteCount = 0
			staticSpriteCount = 0
			mapTriggers = OrderedDict()
			mapDoors = OrderedDict()
			mapQuests = OrderedDict()
			questCount = 0
			mapSpecialWalls = OrderedDict()
			specialWallCount = 0

			with open(map_filename) as mdatafile, open(output_dir + "level%u" % level + ".bin", 'wb') as bfile, open(output_dir + "level%u" % level + ".txt", 'w') as tfile, open(output_dir + "level%u_quests" % level + ".bin", "wb") as qbfile, open(output_dir + "level%u_specialWalls" % level + ".bin", "wb") as swfile:
				while True:
					mdata = readLine(mdatafile)

					if not mdata:
						break
					lines += 1
					mapData += mdata

					if lines == mapHeight:
						break

				# doors
				while True:
					line = mdatafile.readline()
					if not line:
						break
					if "Doors" in line:
						lines = 0
						while True:
							line = mdatafile.readline()
							if not line.rstrip('\n'):
								break;
							if "none" in line:
								break
							if line.startswith('#'):
								continue
							createDoor(line)
							lines += 1
						break

				# triggers
				while True:
					line = mdatafile.readline()
					if not line:
						break
					if "Triggers" in line:
						lines = 0
						while True:
							line = mdatafile.readline()
							if not line.rstrip('\n'):
								break;
							if "none" in line:
								break
							if line.startswith('#'):
								continue
							createTrigger(line)
							lines += 1
						break

				# moving walls
				while True:
					line = mdatafile.readline()
					if not line:
						break
					if "Moving Walls" in line:
						lines = 0
						while True:
							line = mdatafile.readline()
							if not line.rstrip('\n'):
								break;
							if "none" in line:
								break
							if line.startswith('#'):
								continue
							createMovingWall(line)
							lines += 1
						break

				while True:
					line = mdatafile.readline()
					if not line:
						break
					if "Sprites" in line:
						lines = 0
						while True:
							line = mdatafile.readline()
							if not line.rstrip('\n'):
								break;
							if "none" in line:
								break
							if line.startswith('#'):
								continue
							createSprite(line)
							lines += 1
						break

				while True:
					line = mdatafile.readline()
					if not line:
						break
					if "Quests" in line:
						lines = 0
						while True:
							line = mdatafile.readline()
							if not line.rstrip('\n'):
								break;
							if "none" in line:
								break
							if line.startswith('#'):
								continue
							createQuest(line)
							lines += 1
						break

				# special walls
				while True:
					line = mdatafile.readline()
					if not line:
						break
					if "Special" in line:
						lines = 0
						while True:
							line = mdatafile.readline()
							if not line.rstrip('\n'):
								break;
							if "none" in line:
								break
							if line.startswith('#'):
								continue
							createSpecialWall(line)
							lines += 1
						break

				# walk the whole map and try to execute any special function
				for mapX in range(mapWidth):
					for mapY in range(mapHeight):
						offset = mapY * mapWidth + mapX
						try:
							fn = special_fn[mapData[offset]]
							fn(hfile, mapData[offset], mapX, mapY)
						except KeyError:
							pass

				# write map data
				save_binary_data(mapData, bfile)

				#
				# write triggers init data
				#
				for tk in list(mapTriggers.keys()):
					t = mapTriggers[tk]
					bfile.write(struct.pack('<B', t["flags"]))
					bfile.write(struct.pack('<B', t["obj-id"]))
					bfile.write(struct.pack('<B', t["x"]))
					bfile.write(struct.pack('<B', t["y"]))
					bfile.write(struct.pack('<B', t["blockid"]))
					bfile.write('\0'.encode('utf-8'))  # pad

				if triggerCount > maxTriggerCount:
					print(("Error: Map " + map_filename + " has too many triggers\n"))
					sys.exit()

				# pad up to max trigger count
				for pad in range(maxTriggerCount - triggerCount):
					bfile.write('\0'.encode('utf-8') * 6)

				# write moving walls init data
				for mwk in list(movingWalls.keys()):
					mwd = movingWalls[mwk]
					bfile.write(struct.pack('<B', mwd["flags"]))
					bfile.write(struct.pack('<B', mwd["map-x"]))
					bfile.write(struct.pack('<B', mwd["map-y"]))
					bfile.write(struct.pack('<B', mwd["min-y"]))
					bfile.write(struct.pack('<B', mwd["max-y"]))
					bfile.write(struct.pack('<B', mwd["blockid"]))
					bfile.write('\0'.encode('utf-8'))  # pad

				if movingWallCount > maxMovingWallCount:
					print(("Error: Map " + map_filename + " has too many moving walls\n"))
					sys.exit()

				# pad up to max moving wall count
				for pad in range(maxMovingWallCount - movingWallCount):
					bfile.write('\0'.encode('utf-8') * 7)

				# write doors init data
				for dk in list(mapDoors.keys()):
					d = mapDoors[dk]
					bfile.write(struct.pack('<B', d[0]))
					bfile.write(struct.pack('<B', d[1]))
					bfile.write(struct.pack('<B', d[2]))
					bfile.write('\0'.encode('utf-8') * 3)  # pad

				if doorCount > maxDoorCount:
					print(("Error: Map " + map_filename + " has too many doors\n"))
					sys.exit()

				# pad up to max door count
				for pad in range(maxDoorCount - doorCount):
					bfile.write('\0'.encode('utf-8') * 6)

				#################################################################
				# write dynamic sprites init data
				#
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

				if spriteCount > maxSpriteCount:
					print(("Error: Map " + map_filename + " has too many sprites (%u/%u)\n" % (spriteCount, maxSpriteCount)))
					sys.exit()

				# pad up to max sprite count
				for pad in range(maxSpriteCount + maxProjectileCount - spriteCount):
					bfile.write(struct.pack('<BBB', 0, 0, 0))
					bfile.write(struct.pack('<B', 1))  # flags: inactive

				#################################################################
				# write dynamic sprites health table
				#
				for spr in list(mapSprites.keys()):
					s = mapSprites[spr]
					bfile.write(struct.pack('<B', s["health"]))

				# pad up to max sprite count
				for pad in range(maxSpriteCount - spriteCount):
					bfile.write(struct.pack('<B', 0))


				#################################################################
				# write static sprites init data
				#

				for spr in list(mapStaticSprites.keys()):
					s = mapStaticSprites[spr]
					_id = s["id"]
					# compose flags
					flags = 1                  # active
					flags |= 0x80              # non-shootable
					flags |= s["type"] << 1    # type
					bfile.write(struct.pack('<B', flags))

				if staticSpriteCount > maxStaticSpriteCount:
					print(("Error: Map " + map_filename + " has too many static sprites (%u/%u)\n" % (staticSpriteCount, maxStaticSpriteCount)))
					sys.exit()

				# pad up to max sprite count
				for pad in range(maxStaticSpriteCount - staticSpriteCount):
					bfile.write(struct.pack('<B', 0))



				#################################################################
				# write additional map information
				bfile.write(struct.pack('<B', spriteCount))
				bfile.write(struct.pack('<B', len(mapSpecialWalls))) # save number of special walls

				#################################################################
				# write players init data
				bfile.write(struct.pack('<H', playerX))
				bfile.write(struct.pack('<H', playerY))
				bfile.write(struct.pack('<B', int(playerX / blockSize)))
				bfile.write(struct.pack('<B', int(playerY / blockSize)))
				bfile.write(struct.pack('<h', 0))

				# TODO player health, ammo and the like?

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
					sys.exit()

				#
				# write special wall data
				#
				for sw in list(mapSpecialWalls.keys()):
					s = mapSpecialWalls[sw]
					mapX = s["map-x"]
					swfile.write(struct.pack('<B', mapX))
					mapY = s["map-y"]
					swfile.write(struct.pack('<B', mapY))
					swfile.write(struct.pack('<B', s["flags"]))

				mapSizeBytes = len(mapData)
				mapTriggerSizeBytes = len(mapTriggers) * 5
				movingWallInitDataSizeBytes = len(list(movingWalls.keys())) * 6
				doorsInitDataSizeBytes = len(list(mapDoors.keys())) * 3
				spritesInitDataSizeBytes = len(mapSprites) * 4
				questSizeBytes = len(mapQuests) * 18
				specialWallsSizeBytes = len(mapSpecialWalls) * 3 + 1

				sumMapSize = mapSizeBytes
				sumMapSize += mapTriggerSizeBytes
				sumMapSize += movingWallInitDataSizeBytes
				sumMapSize += doorsInitDataSizeBytes
				sumMapSize += spritesInitDataSizeBytes
				sumMapSize += questSizeBytes
				sumMapSize += specialWallsSizeBytes

				# TODO count enemies, items

				tfile.write("/*\n")
				tfile.write(" * ==================================================\n")
				tfile.write(" *  Summary for %s\n" % (map_filename))
				tfile.write(" * ==================================================\n")
				tfile.write(" *   triggers                                    : %u\n" % (triggerCount))
				tfile.write(" *   doors                                       : %u\n" % (doorCount))
				tfile.write(" *   moving walls                                : %u\n" % (movingWallCount))
				tfile.write(" *   sprites                                     : %u\n" % (spriteCount))
				tfile.write(" *   static sprites                              : %u\n" % (staticSpriteCount))
				tfile.write(" *   quests                                      : %u\n" % (questCount))
				tfile.write(" *   special walls                              : %u\n" % (specialWallCount))
				tfile.write(" * ----------------------------------------------------\n")
				tfile.write(" *   memory used for map                  (bytes): %u\n" % (mapSizeBytes))
				tfile.write(" *   memory used for triggers             (bytes): %u\n" % (mapTriggerSizeBytes))
				tfile.write(" *   memory used for moving walls         (bytes): %u\n" % (movingWallInitDataSizeBytes))
				tfile.write(" *   memory used for doors                (bytes): %u\n" % (doorsInitDataSizeBytes))
				tfile.write(" *   memory used for sprites              (bytes): %u\n" % (spritesInitDataSizeBytes))
				tfile.write(" *   memory used for quests               (bytes): %u\n" % (questSizeBytes))
				tfile.write(" *   memory used for special walls       (bytes): %u\n" % (specialWallsSizeBytes))
				tfile.write(" * ----------------------------------------------------\n")
				tfile.write(" *   sum                                  (bytes): %u\n" % (sumMapSize))
				tfile.write(" * ====================================================\n")
				tfile.write(" */\n")


		# create a define for the number of levels
		hfile.write("#define MAX_LEVELS           %u\n" % level)

		# finalize the .h file
		hfile.write("\n\n")
		hfile.write("#endif")

