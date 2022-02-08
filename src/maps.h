#ifndef __MAPS_H
#define __MAPS_H

/*
 * floor
 */
#define F0  0
/*
 * wall blocks, each 4 sides
 * the texture on the sides depends on another array that defines them
 */
#define W0     1                           // just a regular wall block
#define W1     2                           // ...
#define W2     3                           // ...
#define W3     4                           // ...
#define W4     5                           // ...
#define W5     6                           // ...
#define W6     7                           // ...
#define W7     8                           // ...
#define W8     9
#define W9     10
#define W10    11
#define W11    12
#define W12    13
#define W13    14
#define W14    15
#define WALL_BLOCKS    31

/* half wall blocks, must be surrounded by regular blocks */
#define HALF_BLOCKS_START   0x20
#define w0     0x21
#define w1     0x22
#define w2     0x23
#define w3     0x24
#define w4     0x25
#define w5     0x26
#define w6     0x27
#define w7     0x28
#define w8     0x29
#define w9     0x2a
#define w10    0x2b
#define w11    0x2c
#define w12    0x2d
#define w13    0x2e
#define H_DOOR               0x3f            /* horizontal/vertical door */

/*
 * special tags like doors, trigger, moving walls
 */
#define TRIGGER              0x40            /* to trigger some action */
#define V_M_W                0x41            /* vertical moving wall block */
#define SHOOTING_WALL_L      0x42            /* wall that can shoot (left) */
#define SHOOTING_WALL_R      0x43            /* wall that can shoot (right) */
#define SHOOTING_WALL_U      0x44            /* wall that can shoot (up) */
#define SHOOTING_WALL_D      0x45            /* wall that can shoot (down) */

#define V_DOOR               0x46            /* vertical door, only used internally, do not use in maps */

/*
 * fixed sprites
 */
#define SPRITES_START        0x80
#define S_TYPE_FIRST         0x80
#define S_TYPE0              S_TYPE_FIRST
#define S_TYPE1              0x81
#define S_TYPE_LAST          S_TYPE1
#define S_TYPE_MAX_NR        (S_TYPE_LAST - S_TYPE_FIRST)

#define ENEMIES_START        0x82
#define E_TYPE_FIRST         0x82
#define E_TYPE0              E_TYPE_FIRST
#define E_TYPE1              0x83
#define E_TYPE2              0x84
#define E_TYPE3              0x85
#define E_TYPE_LAST          E_TYPE3
#define E_TYPE_MAX_NR        (E_TYPE_LAST - E_TYPE_FIRST)
/*
 * items
 */
#define I_TYPE_FIRST         0x86
#define I_TYPE0              I_TYPE_FIRST   /* the key */
#define I_TYPE1              0x87           /* collectable weapon 1 */
#define I_TYPE2              0x88           /* collectable weapon 2 */
#define I_TYPE3              0x89           /* collectable weapon 3 */
#define I_TYPE4              0x8a           /* collectable health */
#define I_TYPE5              0x8b           /* collectable ammo */
#define I_TYPE_LAST          I_TYPE5
#define STATUS_MSG_OFFSET    I_TYPE_FIRST   /* xlate from map id to message id */
#define I_TYPE_MAX_NR        (I_TYPE_LAST - I_TYPE_FIRST)

#define V_OF_S(i)            ((i) - SPRITES_START)   /* macro to get offset cleaned value of sprite */

#define FOG                  0xef
/*----------------------------------------------------------
 * enemy configuration
 *----------------------------------------------------------*/
#define ENEMY0_MOVEMENT_SPEED        4
#define ENEMY1_MOVEMENT_SPEED        6
#define ENEMY2_MOVEMENT_SPEED        6
#define ENEMY3_MOVEMENT_SPEED        8

/*----------------------------------------------------------
 * sprite configuration
 *----------------------------------------------------------*/
#define MAX_VISIBLE_SPRITES    10 // TODO ?

#define F_MASK                 0x3
#define S_ENEMY                (0 << 0)
#define S_SIMPLE               (1 << 0)
#define S_PROJECTILE           (2 << 0)
#define IS_ENEMY(v)            (((v) & F_MASK) == S_ENEMY)
#define IS_SIMPLE(v)           (((v) & F_MASK) == S_SIMPLE)
#define IS_PROJECTILE(v)       (((v) & F_MASK) == S_PROJECTILE)

#define S_INACTIVE             (1 << 2)
#define IS_INACTIVE(v)         ((v) & S_INACTIVE)

/* sprite dimensions in pixel */
#define SPRITE_WIDTH           16
#define SPRITE_HEIGHT          16
#define SPRITE_HEIGHT_BYTES    ((SPRITE_HEIGHT + 7) / 8)
#define SPRITE_SIZE            (SPRITE_HEIGHT_BYTES * SPRITE_WIDTH)


/*----------------------------------------------------------
 * player configuration
 *----------------------------------------------------------*/
#define PLAYERS_DAMAGE_TIMEOUT              16 // timeout in frames until damage is applied (should be power of 2)
#define PLAYERS_MIN_WALL_DISTANCE           16                          // pixel, minimum distance of the player to a wall
#define PLAYERS_SPEED                        8   /* pixel, must not be bigger than the blocksize, power of two */
#define PLAYERS_INITIAL_HEALTH              100  /* players initial health, should be [1 .. 100] */

/*----------------------------------------------------------
 * enemy configuration
 *----------------------------------------------------------*/
/* threshold where enemies will do an successful attack */
#define ENEMY_ATTACK_THRESHOLD     50

/*----------------------------------------------------------
 * door configuration
 *----------------------------------------------------------*/
/* door is open for 8s */
#define DOOR_OPEN_TIMEOUT                                (FPS * 8)
/* maximum number of flaky doors to activate at once */
#define MAX_NR_OF_FLAKY_DOORS_TO_ACTIVATE                3
/*
 * door:
 *   OPENING -> OPEN -> CLOSING -> CLOSED
 */
#define DOOR_CLOSED        0
#define DOOR_OPENING       1
#define DOOR_OPEN          2
#define DOOR_CLOSING       3

#define DOOR_FLAG_FLAKY    (1 << 0)  /* flaky doors open/close randomly */
#define DOOR_FLAG_LOCKED   (1 << 1)  /* need a key to open this */
#define DOOR_FLAG_TRIGGER  (1 << 2)  /* need a trigger to open this */

/*----------------------------------------------------------
 * misc configuration
 *----------------------------------------------------------*/
/* number of blocks that can be ignored, should be at least the number of doors */
#define MAX_IGNORE_BLOCKS           5
#if MAX_IGNORE_BLOCKS < MAX_DOORS
#error "number of ignore blocks must be equal or bigger than the number of doors"
#endif

/*----------------------------------------------------------
 * moving walls configuration
 *----------------------------------------------------------*/
#define MW_DIRECTION_INC       (1 << 0)

#define VMW_FLAG_DIRECTION     (1 << 0)
#define VMW_FLAG_DAMAGE        (1 << 5)
#define VMW_FLAG_ACTIVE        (1 << 6)
#define VMW_FLAG_ONESHOT       (1 << 7)

/*----------------------------------------------------------
 * trigger configuration
 *----------------------------------------------------------*/
/*
 * for now align to door open timeout so for switches
 * it will look like the switch toggles back when the
 * door closes
 */
#define TRIGGER_TIMEOUT       DOOR_OPEN_TIMEOUT

#define TRIGGER_FLAG_STATE    (1 << 0)
#define TRIGGER_STATE_ON      (1 << 0)
#define TRIGGER_STATE_OFF     (0 << 0)

#define TRIGGER_FLAG_TYPE     (1 << 1)
#define TRIGGER_TYPE_ONE_SHOT (0 << 1)
#define TRIGGER_TYPE_SWITCH   (1 << 1)

#define TRIGGER_FLAG_OBJ        (0x7 << 2)
#define TRIGGER_OBJ_DOOR        (0x0 << 2)
#define TRIGGER_OBJ_VMW         (0x1 << 2)
#define TRIGGER_OBJ_DIALOG      (0x2 << 2)
#define TRIGGER_OBJ_NEXT_LEVEL  (0x3 << 2)
#define TRIGGER_OBJ_QUEST       (0x4 << 2)

/*----------------------------------------------------------
 * quest configuration
 *----------------------------------------------------------*/
#define QUEST_NOT_ACTIVE         0xff


#endif
