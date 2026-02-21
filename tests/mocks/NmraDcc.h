#ifndef NMRADCC_MOCK_H
#define NMRADCC_MOCK_H

#include <stdint.h>

typedef uint8_t DCC_ADDR_TYPE;
typedef uint8_t DCC_DIRECTION;
typedef uint8_t DCC_SPEED_STEPS;

#define DCC_ADDR_SHORT 0
#define DCC_ADDR_LONG 1

#define DCC_DIR_REV 0
#define DCC_DIR_FWD 1

#define SPEED_STEP_14 15
#define SPEED_STEP_28 29
#define SPEED_STEP_128 127

#define MAX_DCC_MESSAGE_LEN 6

typedef struct
{
    uint8_t Size ;
    uint8_t PreambleBits ;
    uint8_t Data[MAX_DCC_MESSAGE_LEN] ;
} DCC_MSG ;

typedef enum
{
    FN_0_4 = 1,
    FN_5_8,
    FN_9_12,
    FN_13_20,
    FN_21_28,
    FN_LAST
} FN_GROUP;

#define FN_BIT_00       0x10
#define FN_BIT_01       0x01
#define FN_BIT_02       0x02
#define FN_BIT_03       0x04
#define FN_BIT_04       0x08

#define FN_BIT_05       0x01
#define FN_BIT_06       0x02
#define FN_BIT_07       0x04
#define FN_BIT_08       0x08

#define FN_BIT_09       0x01
#define FN_BIT_10       0x02
#define FN_BIT_11       0x04
#define FN_BIT_12       0x08

#define FN_BIT_13       0x01
#define FN_BIT_14       0x02
#define FN_BIT_15       0x04
#define FN_BIT_16       0x08
#define FN_BIT_17       0x10
#define FN_BIT_18       0x20
#define FN_BIT_19       0x40
#define FN_BIT_20       0x80

#define FN_BIT_21       0x01
#define FN_BIT_22       0x02
#define FN_BIT_23       0x04
#define FN_BIT_24       0x08
#define FN_BIT_25       0x10
#define FN_BIT_26       0x20
#define FN_BIT_27       0x40
#define FN_BIT_28       0x80

#define MAN_ID_DIY              0x0D

class NmraDcc {
public:
  uint16_t getAddr() { return 3; }
  int getCV(int cv) { return 0; }
  void setCV(int cv, int val) {}
  void pin(uint8_t pin, uint8_t pullup) {}
  void init(uint8_t manId, uint8_t verId, uint8_t flags, uint8_t addr) {}
  uint8_t process() { return 0; }
};

#endif
