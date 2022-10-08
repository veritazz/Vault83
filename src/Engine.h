#ifndef __ENGINE_H
#define __ENGINE_H

#include "VeritazzExtra.h"
#include "maps.h"
#include "leveldata.h"
#include "ArduboyFX.h"
#include "EngineData.h"

//#define AUDIO
/*
 * enable some simple cache when reading map data
 */
#define CONFIG_MAP_CACHE
//#define SERIAL_DEBUG
//#define GPIO_DEBUG
#define CONFIG_FPS_MEASUREMENT
#define CONFIG_GOD_MODE
/*
 * support level of detail rendering, each texture will be available in
 * size 1, 1/2, 1/4
 */
//#define CONFIG_LOD

/*
 * TODO these macros are problematic, casting in the formulas below is not
 * ok in all cases
 */
#define pgm_read_int8(p)   ((int8_t)pgm_read_byte((p)))
#define pgm_read_uint8(p)  ((uint8_t)pgm_read_byte((p)))
#define pgm_read_uint16(p) ((uint16_t)pgm_read_word((p)))
#define pgm_read_int16(p)  ((int16_t)pgm_read_word((p)))
#define pgm_cosByX(i)      ((int8_t)pgm_read_int8(&cosByX[(i)]))
#define pgm_fineCosByX(i)  ((uint16_t)pgm_read_uint16(&fineCosByX[(i)]))
#define pgm_tanByX(i)      ((int16_t)pgm_read_uint16(&tanByX[(i)]))

/*---------------------------------------------------------------------------
 * game parameters
 *---------------------------------------------------------------------------*/
#define FPS                         30                          // desired frame rate


#define SCREEN_WIDTH                WIDTH                       // game screen width
#define SCREEN_HEIGHT               (HEIGHT - 8)                // game screen height - HUD height in pixel
#define HEIGHT_BYTES                (HEIGHT / 8)                // screen height in bytes
#define WIDTH_BYTES                 (WIDTH / 8)                 // screen width in bytes
#define SCREEN_HEIGHT_BYTES         (SCREEN_HEIGHT / 8)         // game screen height in bytes
#define SCREEN_WIDTH_BYTES          (SCREEN_WIDTH / 8)          // game screen width in bytes

#define MIN_WALL_HEIGHT              5                          // minimum height in pixels where textures are drawn
								// should not exceed MIN_WALL_DISTANCE
#define BLOCK_SIZE                  64                          // each block is 64x64x64 pixel
#define COSBYX                      BLOCK_SIZE                  // multiplier for fixed point numbers,
								// for efficiency reasons it should be equal to the blocksize
#define BYX                         BLOCK_SIZE
#define HALF_BLOCK_SIZE             (BLOCK_SIZE / 2)            // each block is 64x64x64 pixel
#define FIELD_OF_VIEW               64                          // field of view, 64 degrees
#define PLAYER_HEIGHT               (BLOCK_SIZE / 2)            // half of the block size
#define TEXTURE_SCALE_SHIFT         11
#define TEXTURE_SCALE_1             (1 << TEXTURE_SCALE_SHIFT)  // scale factor 1.0
#define TEXTURE_SCALE_2             (2 << TEXTURE_SCALE_SHIFT)  // scale factor 2.0
#define TEXTURE_SCALE_4             (4 << TEXTURE_SCALE_SHIFT)  // scale factor 4.0
#define TEXTURE_SCALE_UP_LIMIT      512                         /* limit till when to use the optimized scale up function */
#define TEXTURE_WIDTH               32                          /* width of the texture in pixels */
#define TEXTURE_HEIGHT              32                          /* height of the texture in pixels */
#define TEXTURE_HEIGHT_BYTES        ((TEXTURE_HEIGHT + 7) / 8)  /* height of the texture in bytes */
#define TEXTURE_SIZE                (TEXTURE_WIDTH * TEXTURE_HEIGHT_BYTES)    /* texture size in bytes */
/*
 * players distance to projection plane = 64 / tan(FOV/2)
 */
#define DIST_TO_PROJECTION_PLANE    102
/*
 * divider of a step for door rendering, this defines how "deep"
 * the door looks compared to other walls, should be a power of
 * 2
 */
#define DOOR_STEP_DIV_BY            2
/*
 * divider of a step for half block rendering, this defines how
 * "deep" the door looks compared to other walls, should be a
 * power of 2
 */
#define HALF_BLOCK_STEP_DIV_BY      4

/*
 * two bits define viewing direction (quadrant)
 *
 * bit 0: 0 = down, 1 = up    (vertical)
 * bit 1: 0 = left, 1 = right (horizontal)
 */
#define VD_DOWN_RIGHT          0x2
#define VD_DOWN_LEFT           0x0
#define VD_UP_RIGHT            0x3
#define VD_UP_LEFT             0x1

#define TEXTURE_LEFT_TO_RIGHT  0 // texture orientation from left to right
#define TEXTURE_RIGHT_TO_LEFT  1 // texture orientation from right to left

/* enemy does nothing */
#define ENEMY_IDLE                 0
/* enemy attacks player (distance/close?) */
#define ENEMY_ATTACK               1
/* enemy approaches a point on a map */
#define ENEMY_FOLLOW               2
/* enemy does some random movement at its current place */
#define ENEMY_RANDOM_MOVE          3

enum system_events {
	EVENT_NONE              = 0,
	EVENT_PLAYER_DEAD       = 1,
	EVENT_PLAYER_NEXT_LEVEL = 2,
	EVENT_DIALOG            = 4,
	EVENT_QUEST             = 8,

	EVENT_MAX_NR            = 8,
};

struct intersect {
	int16_t ip;       /* intersection point */
	uint16_t ml;      /* movement limit */
	uint16_t ulp;     /* upper left point */
	uint16_t lrp;     /* lower right point */
	uint16_t oX, oY;  /* original x and y position before movement */
	uint8_t mmd;      /* maximum movement distance */
	uint8_t s;        /* 0 = x intersection, 1 = y intersection */
	uint8_t quadrant; /* quadrant the movement is targeting to */
	uint8_t cos;      /* cosinus of the movement angle */
	uint8_t sin;      /* sinus of the movement angle */
	uint8_t distance; /* targeted movement distance */
};

/*
 * Map caching to improve random access on the map that is stored
 * in external flash memory
 *
 * It is assumed that the engine will hit blocks multiple times.
 * So if we can hold the tiles in cache and only need to load a tile
 * from flash from time to time the performance should be much better.
 *
 * The cache is stored in first 64 columns of the framebuffer.
 *
 * Cache organization:
 *  8*16 (8 rows, 16 columns)
 *
 * Addressing:
 *  cache row = mapX % 8 (result is 0..7)
 *  cache column = mapY % 16 (result is 0..15)
 *
 * Cache entry size is 3 bytes so each cache row could hold 21 entries
 * but to make the calculations simpler it is limited to 16
 */
#define CACHE_ROWS                8
#define CACHE_COLUMNS             16

struct cache_entry {
	uint8_t mapX;  /* map x position */
	uint8_t mapY;  /* map y position */
	uint8_t tile;  /* tile number at mapX/mapY */
	uint8_t dummy; /* every 4th byte needs to be preserved */
};

struct quest {
	uint8_t itemType;                /* type of item to collect */
	uint8_t itemSuccessCount;        /* number of items for success */
	uint8_t enemyKillType;           /* type of enemy to kill */
	uint8_t enemyKillSuccessCount;   /* number of kills for success */
	uint8_t eventType;               /* type of event */
	uint8_t eventSuccessCount;       /* number of events for success */
};

#define AUDIO_EFFECT_ID_ENVIRONMENT         0
#define AUDIO_EFFECT_ID_WEAPON              1
#define AUDIO_EFFECT_ID_MAP                 2
#define AUDIO_EFFECT_ID_MUSIC               3

#define AUDIO_EFFECT_TRIGGER_START          1
#define AUDIO_EFFECT_TRIGGER_STOP           2

struct audio_effect {
	uint8_t trigger:2;
	uint8_t data:6;
};

#define SHOOTING_WALL_COOLDOWN              FPS * 1 /* 3 seconds */

class Engine
{
public:
	Engine(Arduboy2Ex &a);
	void init(void);
	/*
	 * this must not be inlined otherwise the stack usage will be
	 * too high and lead to weird crashes
	 */
	void render(void) __attribute__ ((noinline));
	void update(void);
	void drawBitmap(uint8_t x, uint8_t y, const uint24_t bitmap, uint8_t w, uint8_t h, uint8_t color);
	uint8_t drawString(uint8_t x, uint8_t page, uint24_t message);

	/*
	 * system event methods
	 */
	void resetSystemEvents(void);
	void setSystemEvent(uint8_t event, uint8_t data);
	uint8_t getSystemEventData(uint8_t event);
	uint8_t getSystemEvents(void);

	/*
	 * level handling
	 */
	uint8_t jumpToLevel(uint8_t level);
	uint8_t nextLevel(void);

	/*
	 * quest handling
	 */
	uint8_t setActiveQuest(uint8_t questId);
	uint8_t isActiveQuestFinished(void);
	void evaluateActiveQuest(void);
	void rewardActiveQuest(void);

	uint8_t simulation;
	/*
	 * usually the players viewpoint is exactly in the middle of the screen, with the
	 * below shift the view of the player can be moved up/down to simulate crouching,
	 * jumping or the illusion of head movement
	 */
	int8_t vMove;

private:
	Arduboy2Ex *arduboy;

	uint8_t pressed(uint8_t);
	uint8_t simulateButtons(uint8_t);

	void updateSprites(int16_t screenYStart, uint16_t fovLeft, uint16_t maxRayLength);
	void handleSprites(uint16_t rayLength, uint16_t fovLeft, struct renderInfo *re);

	uint8_t setVMWFlags(uint8_t block, struct movingWall *cmw, uint8_t mapX, uint8_t mapY);
	void updateMoveables(void);
	void updateSpecialWalls(void);
	void updateTextureEffects(void);
	void updateTriggers(void);
	void updateDoors(void);

	/* some math accelerators */
	uint16_t fastMul128(uint8_t value);
	uint8_t divU8By8(uint8_t dividend);
	uint8_t divU16ByBlocksize(uint16_t dividend) __attribute__ ((noinline));
	int16_t divS24ByBlocksize(int32_t dividend) __attribute__ ((noinline));
	uint16_t divU24ByBlocksize(uint32_t dividend) __attribute__ ((noinline));
	uint8_t bitreverseU8(uint8_t byte);

	uint8_t initMovingWall(uint8_t mapX, uint8_t mapY, uint8_t block, uint8_t state);
	uint8_t checkIfDoorToBeIgnored(uint8_t mapX, uint8_t mapY);

	uint16_t arc_s8(int8_t value, const int8_t *table, uint16_t tsize);
	uint16_t arc_u16(uint16_t value, const uint16_t *table, uint8_t tsize);
#ifndef CONFIG_ASM_OPTIMIZATIONS
	uint8_t checkIgnoreBlock(uint8_t mapX, uint8_t mapY);
#endif
	uint8_t checkIfMovingWallHit(uint8_t mapX, uint8_t mapY, uint16_t hX, uint16_t hY);
	uint8_t checkIgnoreBlockFast(uint8_t mapX, uint8_t mapY);
	uint8_t checkIgnoreBlockInner(uint8_t pMapX, uint8_t pMapY, uint8_t run);
	int8_t checkDoor(uint8_t wallX);
	void findAndActivateDoor(void);
	void activateDoor(struct door *d, uint8_t door);
	void activateTrigger(uint8_t mapX, uint8_t mapY);
	void cleanIgnoreBlock(void);

	uint8_t checkSolidBlockCheap(uint8_t mapX, uint8_t mapY);

	uint8_t calcDistance(uint16_t pA, uint16_t pB);
	void checkIntersect(struct intersect *i); // __attribute__ ((always_inline));
	uint8_t move(uint16_t viewAngle, uint8_t direction, uint16_t *x, uint16_t *y, uint8_t distance);

	uint8_t moveSprite(struct current_sprite *cs, uint8_t speed);
	void drawNumber(uint8_t x, uint8_t y, uint8_t number);
	void setStatusMessage(uint8_t msg_id);
	void ultraDraw(unsigned char *buffer, uint8_t drawCase);
	void drawNoTexture(int16_t screenY, uint16_t wallHeight, struct renderInfo *re);

	uint8_t movingWallCheckHit(uint8_t mapX, uint16_t a);

	void doDamageForVMW(struct movingWall *mw);
	uint8_t movingWallPushBack(uint16_t playerX, uint16_t *playerY, struct movingWall *mw, uint8_t flags);

	void doDamageToSprite(uint8_t id, uint8_t damage);
	void checkAndDoDamageToSpriteByObjects(uint8_t id);

	/* enemy states */
	uint8_t enemyStateIdle(struct current_sprite *cs, uint8_t speed);
	uint8_t enemyStateAttack(struct current_sprite *cs, uint8_t speed);
	uint8_t enemyStateFollow(struct current_sprite *cs, uint8_t speed);
	uint8_t enemyStateRandomMove(struct current_sprite *cs, uint8_t speed);
	/* enemy movement helpers */
	void enemyTurnToPlayer(struct current_sprite *cs);

	/*
	 * texture effect functions, works on the texture data
	 */
	void textureEffectNone(const uint24_t p, uint8_t texX);
	void textureEffectVFlip(const uint24_t p, uint8_t texX);
	void textureEffectHFlip(const uint24_t p, uint8_t texX);
	void textureEffectInvert(const uint24_t p, uint8_t texX);
	void textureEffectRotateLeft(const uint24_t p, uint8_t texX);

	/*
	 * texture effect, works on the texture index
	 */
	void texturesRotateLeft(uint8_t offset);
	void texturesRotateRight(uint8_t offset);
	void texturesExchangeUpDown(uint8_t offset);
	void texturesExchangeLeftRight(uint8_t offset);

	/* audio functions */
	void startAudioEffect(uint8_t id, uint8_t data);
	void stopAudioEffect(uint8_t id);

	/* misc helper functions */
	uint8_t compareGreaterOrEqual(uint8_t index1, uint8_t index2);
	uint8_t readThroughCache(uint8_t mapX, uint8_t mapY);

	/*
	 * quest data
	 * TODO move into leveldata so it can be saved
	 */
	uint8_t activeQuestId;                          /* id of active quest, 0xff indicates that no quest is active */
	uint8_t questsFinished[(MAX_QUESTS + 7) / 8];   /* bitfield of finished quests: 1 means finished */

	/*
	 * current level number (counting starts at 0)
	 */
	uint8_t currentLevel;

	/*
	 * coolDown for shooting walls
	 */
	uint8_t shootingWallCoolDown;      /* time in frames before the next shot */

	/*
	 * free running attack properties
	 */
	uint8_t attackCoolDown;          /*  cooldown in frames until the next enemy attack */
	uint8_t attackLevel;             /*  current attack level, if threshold is reached the enemy will attack */

	/*
	 * audio effects to play
	 */
	struct audio_effect audioEffects[4];

#ifndef CONFIG_ASM_OPTIMIZATIONS
	uint24_t levelFlashOffset;
#endif
};

#endif
