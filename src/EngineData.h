#ifndef __ENGINE_DATA_H
#define __ENGINE_DATA_H

#include <stdint.h>
#include "leveldata.h"
#include "maps.h"

typedef __uint24 uint24_t;

/* size must be "screen height", e.g. 8 */
struct rayinfo {
	uint16_t rayLength;
	/*
	 * 3 bit blockSideIndex
	 * 1 bit textureOrientation
	 * 2 bit ptrType
	 *
	 */
	uint8_t flags;
	uint8_t wallX;
	uint8_t tile;
	void *ptr;
	uint8_t dummy;
};

struct renderInfo {
	uint8_t ystart;
	uint8_t yend;
	uint8_t yfirst;
	uint8_t ylast;
	uint8_t dummy[4];
};

struct lightweight_sprite {
	uint16_t x;                  // 2  x position on the map
	uint16_t y;                  // 2  y position on the map
	uint8_t type;                // 1  sprite type (e.g. barrel, ammo, enemy)
	int8_t vMove;                // 1  vertical screen offset
	uint8_t health;              // 1  health of the sprite
	uint8_t flags;               // 1  flags indicating if sprite is active and the like
	uint16_t distance;           // 2  distance to the player
	int16_t viewAngle;           // 2  view angle of the sprite
	uint8_t state;               // 1  state of the sprite
	uint8_t hwid;                // 1  index into the heavyweight sprite list
}; /* = 14 bytes */

/*
 * data structure for sprites that are actually drawn on the screen
 */
struct heavyweight_sprite {
	uint24_t p;                  // 3  flash pointer to the sprite graphics
	int16_t screenY;             // 2  screen y coordinate of the sprite (where drawing will start)
	int16_t spriteDisplayHeight; // 2  sprite height in pixel that will be drawn
	uint16_t spriteAngle;        // 2  angle the player is looking at the sprite
	uint8_t visible;             // 1  if potentially visible, >1 if really visible, 0 otherwise
	uint8_t vSide;               // 1  the side the player is looking at, 0 = front, 1 = right, 2 = back, 3 = left
	uint8_t dummy[3];
}; /* = 14 bytes */

/* size = 6 bytes */
struct door {
	/*
	 * bit   0 :  0 = normal door, 1 = flaky door
	 * bit   1 :  1 = locked, 0 otherwise
	 * bit   2 :  1 = need a trigger to open the door, 0 otherwise
	 * bit   3 :  unused
	 * bit[7:4]:  block id of this door (limited to block 0 - 15)
	 */
	uint8_t flags;
	uint8_t mapX;        /* doors map x coordinate */
	uint8_t mapY;        /* doors map y coordinate */
	uint8_t state;       /* doors current state */
	uint8_t offset;      /* open or close offset, 0 = closed, BLOCK_SIZE - 1 = open */
	uint8_t openTimeout; /* time in frames the door stays open */
};

/*
 * size = 7 bytes, keep order as initdata gets copied over it
 */
struct movingWall {
	/*
	 * bit     0: direction
	 * bit [1:4]: speed (only 1,2,4,8)
	 * bit     5: if set, does damage
	 * bit     6: if set, does move
	 * bit     7: if set, wall will only move once in the current direction
	 */
	uint8_t flags;
	uint8_t mapX;     /* current map X */
	uint8_t mapY;     /* current map Y */
	uint8_t min;      /* max map x/y position */
	uint8_t max;      /* min map x/y position */
	uint8_t block_id; /* id of the block of textures for this wall */
	uint8_t offset;   /* current offset in y/x direction */
};

struct trigger {
	/*
	 * bit     0: state: 0 = off, 1 = on
	 * bit     1: type: 0 = one shot, 1 = switch
	 * bit [2:4]: type of obj_id
	 *            0 = door
	 *            1 = moving wall
	 *            2 = dialog
	 *            3 = next level
	 *            x = reserved
	 */
	uint8_t flags;
	uint8_t obj_id;      /* id of the object to trigger */
	uint8_t mapX;        /* triggers x position on the map */
	uint8_t mapY;        /* triggers y position on the map */
	uint8_t block_id;    /* id of the block of textures for this trigger */
	uint8_t timeout;     /* timeout in frames the player needs to wait between activations */
}; // 6 bytes


struct level_initdata {
	struct trigger triggers[MAX_TRIGGERS]; /* list of triggers on the map */
	struct movingWall movingWalls[MAX_MOVING_WALLS]; /* list of moving walls on the map */
	struct door doors[MAX_DOORS]; /* list of doors on the map */
	struct lightweight_sprite lw_sprites[MAX_SPRITES];
	uint16_t playerX;                   /* players x coordinate in pixel */
	uint16_t playerY;                   /* players y coordinate in pixel */
	uint8_t playerMapX;                 /* players current map x position */
	uint8_t playerMapY;                 /* players current map y position */
	int16_t playerAngle;                /* players view angle */
};

#define HAS_WEAPON_1                (1 << 0)
#define HAS_WEAPON_2                (1 << 1)
#define HAS_WEAPON_3                (1 << 2)
#define HAS_WEAPON_4                (1 << 3)
#define NR_OF_WEAPONS               4

struct statusMessage {
	uint24_t text;
	uint8_t timeout;
};


struct engineState {
	uint8_t movementSpeedDuration;      /* timeout in frames until movement stops */
	uint8_t playerActiveWeapon;         /* active weapon of the player */
	int8_t playerHealth;                /* current health of the player */
	uint8_t playerWeapons;              /* bitmask of currently owned weapons of the player */
	uint8_t playerAmmo[NR_OF_WEAPONS];  /* current ammo per weapon */
	uint8_t playerKeys;                 /* current number of collected keys */
	uint8_t direction;                  /* current direction the player is moving, */
	/*
	 * bit 0: 0 = down, 1 = up
	 * bit 1: 0 = left, 1 = right
	 */
	uint8_t viewDirection;              /* players view direction */

					    /* 0 not moving, 1 forward, 2 backward, 3 strafe left, 4 strafe right */
	int8_t turn;                        /* turning direction/speed Q3.4 format signed */
	uint8_t currentDamageCategory;      /* bitmask indication which damage needs to be applied */
	uint8_t weaponCoolDown;             /* timeout to limit fire rate */
	int8_t weaponVisibleOffset;         /* screen offset of the weapon carried by the player */
	struct statusMessage statusMessage; /* current status message */
	uint8_t weaponChangeCooldown;       /* timeount to limit the weapon change rate */

	/* all in Q3.4 format, signed */
	int8_t vMoveDirection;              /* head movement direction ( - = up, + = down), signed Q3.4 */
	int8_t vHeadPosition;               /* current head offset, signed Q3.4 */
	int8_t vHeadPositionTimeout;        /* timeout in frames until when the head jumps back */

	/*
	 * weapon handling
	 */
	uint8_t fireCountdown;              /* if it reaches zero, the weapon fires */
	uint8_t rectOffset;                 /* screen offset of the weapon */

	/*
	 * sprite handling
	 */
	struct heavyweight_sprite hw_sprites[MAX_VISIBLE_SPRITES];
	/*
	 * list of visible sprites, sorted in descending order regarding their distance
	 * to the player
	 */
	uint8_t visibleSpriteList[MAX_SPRITES]; /* list of ids of visible sprites */
	uint8_t nrOfVisibleSprites; /* number of sprites visible by the player */
	/*
	 * trigger handling
	 */
	struct trigger *ct;
	struct trigger *vct;
	struct trigger *hct;

	/*
	 * door and transparent object handling
	 */
	uint8_t ignoreBlock[MAX_IGNORE_BLOCKS]; /* list of ids of blocks to ignore (e.g. open doors, transparent objects */
	uint8_t blocksToIgnore; /* current size of the ignore list */
	uint8_t activeDoors[MAX_DOORS]; /* list of ids of active doors */
	uint8_t nrOfActiveDoors; /* number of doors that are currently active */
	struct door *cd;                       /* current door that was hit by the ray */
	struct door *vcd;                      /* current door the ray hit vertically */
	struct door *hcd;                      /* current door the ray hit horizontally */
	/*
	 * moving walls handling
	 */
	int8_t wallX; /* texture x position for walls */
	int16_t renderRayLength; /* rendering ray length, used for objects that are not on a block boundary */
	uint8_t tempRayAngle; /* players view angle used for cosine calculations */
	uint8_t nTempRayAngle; /* players view angle used for sinus calculations (90 - tempRayAngle) */
	struct movingWall *cmw; /* current moving wall seen by the ray */
	struct movingWall *vcmw; /* current moving wall the ray hit vertically */
	struct movingWall *hcmw; /* current moving wall the ray hit horizontally */
	/*
	 * general
	 */
	uint8_t frame; /* current frame number */
	uint8_t blinkScreen; /* flag to indicate a white screen needs to be rendered */
	uint8_t randomNumber; /* current random number */
	uint8_t doDamageFlags; /* flags indicating which damage needs to be inflicted on the player */
	/*
	 * screen rendering
	 */
	uint8_t textureRotateLeftOffset; /* current offset to rotate textures to the left (texture data manipulation) */
	uint8_t screenColumn[8] __attribute__ ((aligned (8))); /* buffer for one rendered screen column */
	int8_t itemDance; /* current offset for item movement */
	int8_t itemDanceDir; /* current direction for item movement (-1 = up, 1 = down) */
	/*
	 * lower 4 bytes = mask, upper 4 bytes = texture
	 */
	uint8_t texColumn[8] __attribute__ ((aligned (8)));

	/*
	 * System events
	 */
	uint8_t systemEvent;        /* type of system event */
	uint8_t systemEventData[8];    /* event specific data */

	/*
	 * misc
	 */
	uint8_t killedBySprite;      /* id of the sprite + 1 that killed the player */

	/*
	 * level init data
	 */
	struct level_initdata ld;
};

#endif
