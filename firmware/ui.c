/**
 * Copyright (C) 2013, 2014 Johannes Taelman
 *
 * This file is part of Axoloti.
 *
 * Axoloti is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Axoloti is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Axoloti. If not, see <http://www.gnu.org/licenses/>.
 */

#include "axoloti_defines.h"
#include "ui.h"
#include "ch.h"
#include "hal.h"
#include "midi.h"
#include "axoloti_math.h"
#include "patch.h"
#include "sdcard.h"
#include "pconnection.h"
#include "axoloti_board.h"
#include "axoloti_control.h"
#include "ff.h"
#include <string.h>

#define LCD_COL_INDENT 5
#define LCD_COL_RIGHT 0
#define LCD_COL_LEFT 97
#define STATUSROW 7



Btn_Nav_States_struct Btn_Nav_CurStates;
Btn_Nav_States_struct Btn_Nav_PrevStates;
Btn_Nav_States_struct Btn_Nav_Or;
Btn_Nav_States_struct Btn_Nav_And;

int8_t EncBuffer[4];

struct KeyValuePair KvpsHead;
struct KeyValuePair *KvpsDisplay;
struct KeyValuePair *ObjectKvpRoot;
#define MAXOBJECTS 256
struct KeyValuePair *ObjectKvps[MAXOBJECTS];
#define MAXTMPMENUITEMS 15
KeyValuePair_s TmpMenuKvps[MAXTMPMENUITEMS];
KeyValuePair_s ADCkvps[3];

//const char stat = 2;

void SetKVP_APVP(KeyValuePair_s *kvp, KeyValuePair_s *parent,
                 const char *keyName, int length, KeyValuePair_s *array) {
  kvp->kvptype = KVP_TYPE_APVP;
  kvp->parent = (void *)parent;
  kvp->keyname = keyName;
  kvp->apvp.length = length;
  kvp->apvp.current = 0;
  kvp->apvp.array = (void *)array;
}

void SetKVP_AVP(KeyValuePair_s *kvp, KeyValuePair_s *parent,
                const char *keyName, int length, KeyValuePair_s *array) {
  kvp->kvptype = KVP_TYPE_AVP;
  kvp->parent = (void *)parent;
  kvp->keyname = keyName;
  kvp->avp.length = length;
  kvp->avp.current = 0;
  kvp->avp.array = array;
}

void SetKVP_IVP(KeyValuePair_s *kvp, KeyValuePair_s *parent,
                const char *keyName, int *value, int min, int max) {
  kvp->kvptype = KVP_TYPE_IVP;
  kvp->parent = (void *)parent;
  kvp->keyname = keyName;
  kvp->ivp.value = value;
  kvp->ivp.minvalue = min;
  kvp->ivp.maxvalue = max;
}

void SetKVP_IPVP(KeyValuePair_s *kvp, KeyValuePair_s *parent,
                 const char *keyName, ParameterExchange_t *PEx, int min,
                 int max) {
  PEx->signals = 0x0F;
  kvp->kvptype = KVP_TYPE_IPVP;
  kvp->parent = (void *)parent;
  kvp->keyname = keyName;
  kvp->ipvp.PEx = PEx;
  kvp->ipvp.minvalue = min;
  kvp->ipvp.maxvalue = max;
}

void SetKVP_FNCTN(KeyValuePair_s *kvp, KeyValuePair_s *parent,
                  const char *keyName, VoidFunction fnctn) {
  kvp->kvptype = KVP_TYPE_FNCTN;
  kvp->parent = (void *)parent;
  kvp->keyname = keyName;
  kvp->fnctnvp.fnctn = fnctn;
}

void SetKVP_CUSTOM(KeyValuePair_s *kvp, KeyValuePair_s *parent,
                  const char *keyName, DisplayFunction dispfnctn, ButtonFunction btnfnctn, void* userdata) {
  kvp->kvptype = KVP_TYPE_CUSTOM;
  kvp->parent = (void *)parent;
  kvp->keyname = keyName;
  kvp->custom.displayFunction = dispfnctn;
  kvp->custom.buttonFunction = btnfnctn;
  kvp->custom.userdata = userdata;
}



inline void KVP_Increment(KeyValuePair_s *kvp) {
  switch (kvp->kvptype) {
  case KVP_TYPE_IVP:
    if (*kvp->ivp.value < kvp->ivp.maxvalue)
      (*kvp->ivp.value)++;
    break;
  case KVP_TYPE_AVP:
    if (kvp->avp.current < (kvp->avp.length - 1))
      kvp->avp.current++;
    break;
  case KVP_TYPE_APVP:
    if (kvp->apvp.current < (kvp->apvp.length - 1))
      kvp->apvp.current++;
    break;
  case KVP_TYPE_U7VP:
    if (*kvp->u7vp.value < kvp->u7vp.maxvalue)
      (*kvp->u7vp.value) += 1;
    break;
  case KVP_TYPE_IPVP: {
    int32_t nval = kvp->ipvp.PEx->value + (1 << 20);
    if (nval < kvp->ipvp.maxvalue) {
      PExParameterChange(kvp->ipvp.PEx, nval, 0xFFFFFFE7);
    }
    else {
      PExParameterChange(kvp->ipvp.PEx, kvp->ipvp.maxvalue, 0xFFFFFFE7);
    }
  }
    break;
  default:
    break;
  }
}

inline void KVP_Decrement(KeyValuePair_s *kvp) {
  switch (kvp->kvptype) {
  case KVP_TYPE_IVP:
    if (*kvp->ivp.value > kvp->ivp.minvalue)
      (*kvp->ivp.value)--;
    break;
  case KVP_TYPE_AVP:
    if (kvp->avp.current > 0)
      kvp->avp.current--;
    break;
  case KVP_TYPE_APVP:
    if (kvp->apvp.current > 0)
      kvp->apvp.current--;
    break;
  case KVP_TYPE_U7VP:
    if (*kvp->u7vp.value > kvp->u7vp.minvalue)
      (*kvp->u7vp.value)--;
    break;
  case KVP_TYPE_IPVP: {
    int32_t nval = kvp->ipvp.PEx->value - (1 << 20);
    if (nval > kvp->ipvp.minvalue) {
      PExParameterChange(kvp->ipvp.PEx, nval, 0xFFFFFFE7);
    }
    else {
      PExParameterChange(kvp->ipvp.PEx, kvp->ipvp.minvalue, 0xFFFFFFE7);
    }
  }
    break;
  default:
    break;
  }
}


#define POLLENC(NAME, INCREMENT_FUNCTION, DECREMENT_FUNCTION)  \
      if (!expander_PrevStates.NAME##A) {                 \
          if (!expander_PrevStates.NAME##B) {             \
              if (expander_CurStates.NAME##B) {           \
                  expander_PrevStates.NAME##B = 1;        \
                  DECREMENT_FUNCTION                      \
              } else if (expander_CurStates.NAME##A) {    \
                  expander_PrevStates.NAME##A = 1;        \
                  INCREMENT_FUNCTION                      \
              }                                           \
          } else {                                        \
              if (expander_CurStates.NAME##A) {           \
                  expander_PrevStates.NAME##A = 1;        \
              } else if (!expander_CurStates.NAME##B) {   \
                  expander_PrevStates.NAME##B = 0;        \
              }                                           \
          }                                               \
      } else {                                            \
          if (expander_PrevStates.NAME##B) {              \
              if (!expander_CurStates.NAME##B) {          \
                  expander_PrevStates.NAME##B = 0;        \
              } else if (!expander_CurStates.NAME##A) {   \
                  expander_PrevStates.NAME##A = 0;        \
              }                                           \
          } else {                                        \
              if (!expander_CurStates.NAME##A) {          \
                  expander_PrevStates.NAME##A = 0;        \
              } else if (expander_CurStates.NAME##B) {    \
                  expander_PrevStates.NAME##B = 1;        \
              }                                           \
          }                                               \
      }

/*
 * Create menu tree from file tree
 */

uint8_t *memp;
KeyValuePair_s LoadMenu;

void EnterMenuLoadFile(void) {
  KeyValuePair_s *F =
      &((KeyValuePair_s *)(LoadMenu.avp.array))[LoadMenu.avp.current];

  char str[20] = "0:";
  strcat(str, F->keyname);

  sdcard_loadPatch(str);
}

void EnterMenuLoad(void) {
  memp = (uint8_t *)&fbuff[0];
  FRESULT res;
  FILINFO fno;
  DIR dir;
  int index = 0;
  char *fn;
#if _USE_LFN
  fno.lfname = 0;
  fno.lfsize = 0;
#endif
  res = f_opendir(&dir, "");
  if (res == FR_OK) {
    for (;;) {
      res = f_readdir(&dir, &fno);
      if (res != FR_OK || fno.fname[0] == 0)
        break;
      if (fno.fname[0] == '.')
        continue;
      fn = fno.fname;
      if (fno.fattrib & AM_DIR) {
        // ignore subdirectories for now
      }
      else {
        int l = strlen(fn);
        if ((fn[l - 4] == '.') && (fn[l - 3] == 'B') && (fn[l - 2] == 'I')
            && (fn[l - 1] == 'N')) {
          char *s;
          s = (char *)memp;
          strcpy(s, fn);
          memp += l + 1;
          SetKVP_FNCTN(&TmpMenuKvps[index], NULL, s, &EnterMenuLoadFile);
          index++;
        }
      }
    }
    SetKVP_AVP(&LoadMenu, &KvpsHead, "Load SD", index, &TmpMenuKvps[0]);
    KvpsDisplay = &LoadMenu;
  }
  // TBC: error messaging
}

void EnterMenuFormat(void) {
  FRESULT err;
  err = f_mkfs(0, 0, 0);
  if (err != FR_OK) {
    SetKVP_AVP(&TmpMenuKvps[0], &KvpsHead, "Format failed", 0, 0);
    KvpsDisplay = &TmpMenuKvps[0];
  }
  else {
    SetKVP_AVP(&TmpMenuKvps[0], &KvpsHead, "Format OK", 0, 0);
    KvpsDisplay = &TmpMenuKvps[0];
  }
}

#define NEWBOARD 1
#ifndef NEWBOARD
/*
 * This is a periodic thread that handles display/buttons
 */
static void UIPollButtons(void);
static void UIUpdateLCD(void);

void AxolotiControlUpdate(void) {
  static int refreshLCD=0;
    UIPollButtons();
    if (!(0x0F & refreshLCD++)) {
      UIUpdateLCD();
      LCD_display();
    }
}


void (*pControlUpdate)(void) = AxolotiControlUpdate;

static WORKING_AREA(waThreadUI, 2048);
static msg_t ThreadUI(void *arg) {
  while(1) {
    if(pControlUpdate != 0L) {
        pControlUpdate();
    }
    AxoboardADCConvert();
    PollMidiIn();
    PExTransmit();
    PExReceive();
    chThdSleepMilliseconds(2);
  }
}
#else
static void UIUpdateLCD(void);
static void UIPollButtons(void);

void AxolotiControlUpdate(void) {
#if ((BOARD_AXOLOTI_V03)||(BOARD_AXOLOTI_V05))
//    do_axoloti_control();
    UIPollButtons();
    UIUpdateLCD();
#endif
}

void (*pControlUpdate)(void) = AxolotiControlUpdate;

static WORKING_AREA(waThreadUI, 1142);
static msg_t ThreadUI(void *arg) {
  (void)(arg);
#if CH_USE_REGISTRY
  chRegSetThreadName("ui");
#endif
  while (1) {
    PExTransmit();
    PExReceive();
    chThdSleepMilliseconds(2);
  }
  return (msg_t)0;
}
#endif

static WORKING_AREA(waThreadUI2, 512);
static msg_t ThreadUI2(void *arg) {
  (void)(arg);
#if CH_USE_REGISTRY
  chRegSetThreadName("ui2");
#endif
  while (1) {
    if(pControlUpdate != 0L) {
        pControlUpdate();
    }
    chThdSleepMilliseconds(15);
  }
  return (msg_t)0;
}


void UIGoSafe(void) {
  KvpsDisplay = &KvpsHead;
}

static KeyValuePair_s* userDisplay;

void UISetUserDisplay(DisplayFunction dispfnctn, ButtonFunction btnfnctn, void* userdata) {
	if(userDisplay!=0) {
		userDisplay->custom.displayFunction = dispfnctn;
		userDisplay->custom.buttonFunction = btnfnctn;
		userDisplay->custom.userdata = userdata;
	}
}

void ui_init(void) {
  Btn_Nav_Or.word = 0;
  Btn_Nav_And.word = ~0;

  KeyValuePair_s *p1 = chCoreAlloc(sizeof(KeyValuePair_s) * 6);
  KeyValuePair_s *q1 = p1;
  SetKVP_FNCTN(q1++, &KvpsHead, "Info", 0);
  SetKVP_FNCTN(q1++, &KvpsHead, "Format", &EnterMenuFormat);
  if (fs_ready)
    SetKVP_FNCTN(q1++, &KvpsHead, "Load SD", &EnterMenuLoad);
  else
    SetKVP_FNCTN(q1++, &KvpsHead, "No SDCard", NULL);

  KeyValuePair_s *p = chCoreAlloc(sizeof(KeyValuePair_s) * 6);
//  KeyValuePair *q = p;
  int entries = 0;

  SetKVP_APVP(&p[entries++], &KvpsHead, "Patch", 0, &ObjectKvps[0]);
  SetKVP_IVP(&p[entries++], &KvpsHead, "Running", &patchStatus, 0, 15);
  SetKVP_AVP(&p[entries++], &KvpsHead, "SDCard Tools", 3, &p1[0]);
  SetKVP_AVP(&p[entries++], &KvpsHead, "ADCs", 3, &ADCkvps[0]);
  SetKVP_IVP(&p[entries++], &KvpsHead, "dsp%", &dspLoadPct, 0, 100);
  userDisplay = &p[entries++];
  SetKVP_CUSTOM(userDisplay, &KvpsHead, "User", NULL,NULL,NULL);

  SetKVP_AVP(&KvpsHead, NULL, "--- AXOLOTI ---", entries, &p[0]);

  KvpsDisplay = &KvpsHead;

  int i;
  for (i = 0; i < 3; i++) {
    ADCkvps[i].kvptype = KVP_TYPE_SVP;
    ADCkvps[i].svp.value = (int16_t *)&adcvalues[i];
    char *str = chCoreAlloc(6);
    str[0] = 'A';
    str[1] = 'D';
    str[2] = 'C';
    str[3] = '0' + i;
    str[4] = 0;
//    sprintf(str,"CC%i",i);
    ADCkvps[i].keyname = str;    //(char *)i;
  }

  ObjectKvpRoot = &p[0];

  chThdCreateStatic(waThreadUI, sizeof(waThreadUI), NORMALPRIO, ThreadUI, NULL);
  chThdCreateStatic(waThreadUI2, sizeof(waThreadUI2), NORMALPRIO, ThreadUI2, NULL);

  axoloti_control_init();

  for(i=0;i<4;i++) {
	  EncBuffer[i]=0;
  }
}

void KVP_ClearObjects(void) {
  ObjectKvpRoot->apvp.length = 0;
  KvpsDisplay = &KvpsHead;
}

void KVP_RegisterObject(KeyValuePair_s *kvp) {
  ObjectKvps[ObjectKvpRoot->apvp.length] = kvp;
//	kvp->parent = ObjectKvpRoot;
  ObjectKvpRoot->apvp.length++;
}

#define LCD_COL_EQ 91
#define LCD_COL_VAL LCD_COL_LEFT
#define LCD_COL_ENTER LCD_COL_LEFT

void KVP_DisplayInv(int x, int y, KeyValuePair_s *kvp) {
  LCD_drawStringInvN(x, y, kvp->keyname, LCD_COL_EQ);
  switch (kvp->kvptype) {
  case KVP_TYPE_U7VP:
    LCD_drawCharInv(LCD_COL_EQ, y, '=');
    LCD_drawNumber3DInv(LCD_COL_VAL, y, (*kvp->u7vp.value));
    break;
  case KVP_TYPE_IVP:
    LCD_drawCharInv(LCD_COL_EQ, y, '=');
    LCD_drawNumber3DInv(LCD_COL_VAL, y, *kvp->ivp.value);
    break;
  case KVP_TYPE_FVP:
    LCD_drawStringInvN(LCD_COL_EQ, y, "     F", LCDWIDTH);
    break;
  case KVP_TYPE_AVP:
    LCD_drawStringInvN(LCD_COL_EQ, y, "     *", LCDWIDTH);
    break;
  case KVP_TYPE_IDVP:
    LCD_drawIBAR(LCD_COL_EQ, y, *kvp->idvp.value, LCDWIDTH);
    break;
  case KVP_TYPE_SVP:
    LCD_drawCharInv(LCD_COL_EQ, y, '=');
    LCD_drawNumber5DInv(LCD_COL_VAL, y, *kvp->svp.value);
    break;
  case KVP_TYPE_APVP:
    LCD_drawStringInvN(LCD_COL_EQ, y, "     #", LCDWIDTH);
    break;
  case KVP_TYPE_CUSTOM:
    LCD_drawStringInvN(LCD_COL_EQ, y, "     @", LCDWIDTH);
    break;
  case KVP_TYPE_IPVP:
    LCD_drawCharInv(LCD_COL_EQ, y, '=');
    LCD_drawNumber3DInv(LCD_COL_VAL, y, (kvp->ipvp.PEx->value) >> 20);
    break;
  default:
    break;
  }
}

void KVP_Display(int x, int y, KeyValuePair_s *kvp) {
  LCD_drawStringN(x, y, kvp->keyname, LCD_COL_EQ);
  switch (kvp->kvptype) {
  case KVP_TYPE_U7VP:
    LCD_drawChar(LCD_COL_EQ, y, '=');
    LCD_drawNumber3D(LCD_COL_VAL, y, (*kvp->u7vp.value));
    break;
  case KVP_TYPE_IVP:
    LCD_drawChar(LCD_COL_EQ, y, '=');
    LCD_drawNumber3D(LCD_COL_VAL, y, *kvp->ivp.value);
    break;
  case KVP_TYPE_FVP:
    LCD_drawStringN(LCD_COL_EQ, y, "     F", LCDWIDTH);
    break;
  case KVP_TYPE_AVP:
    LCD_drawStringN(LCD_COL_EQ, y, "     *", LCDWIDTH);
    break;
  case KVP_TYPE_IDVP:
    LCD_drawIBAR(LCD_COL_EQ, y, *kvp->idvp.value, LCDWIDTH);
    break;
  case KVP_TYPE_SVP:
    LCD_drawChar(LCD_COL_EQ, y, '=');
    LCD_drawNumber5D(LCD_COL_VAL, y, *kvp->svp.value);
    break;
  case KVP_TYPE_APVP:
    LCD_drawStringN(LCD_COL_EQ, y, "     #", LCDWIDTH);
    break;
  case KVP_TYPE_CUSTOM:
    LCD_drawStringN(LCD_COL_EQ, y, "     @", LCDWIDTH);
    break;
  case KVP_TYPE_IPVP:
    LCD_drawChar(LCD_COL_EQ, y, '=');
    LCD_drawNumber3D(LCD_COL_VAL, y, (kvp->ipvp.PEx->value) >> 20);
    break;
  case KVP_TYPE_INTDISPLAY:
    LCD_drawChar(LCD_COL_EQ, y, '=');
    LCD_drawNumber3D(LCD_COL_VAL, y, *kvp->idv.value >> 20);
//      LCD_drawChar(43+24+x,y,'.');
//      LCD_drawChar(43+24+6+x,y,'0'+(((10*(*kvp->idv.value)&0xfffff))>>20));
    break;
  case KVP_TYPE_PITCHDISPLAY:
    LCD_drawChar(LCD_COL_EQ, y, '=');
    LCD_drawNumber3D(LCD_COL_VAL, y, *kvp->pdv.value >> 20);
//      LCD_drawChar(43+24+x,y,'.');
//      LCD_drawChar(43+24+6+x,y,'0'+(((10*(*kvp->pdv.value)&0xfffff))>>20));
    break;
  case KVP_TYPE_FREQDISPLAY: {
    int f10 = ___SMMUL(*kvp->freqdv.value, 48000);
    LCD_drawChar(LCD_COL_EQ, y, '=');
    LCD_drawNumber5D(LCD_COL_VAL, y, f10);
  }
    break;
  case KVP_TYPE_FRACTDISPLAY: {
    int f10 = ___SMMUL(*kvp->fractdv.value, 160);
    LCD_drawChar(LCD_COL_EQ, y, '=');
    LCD_drawNumber3D(LCD_COL_VAL, y, f10 / 10);
    LCD_drawChar(LCD_COL_VAL + 24 + x, y, '.');
    LCD_drawChar(LCD_COL_VAL + 24 + 6 + x, y, '0' + (f10 - (10 * (f10 / 10))));
  }
    break;
  default:
    break;
  }
}



/*
 * We need one uniform state for the buttons, whether controlled from the GUI or from Axoloti Control.
 * btn_or is a true if the button was down during the last time interval
 * btn_and is false if the button was released after being held down in the last time interval
 *
 * say btn_or1, btn_and1 is from control source 1
 * say btn_or2, btn_and2 is from control source 2
 *
 * Current state |= (btn_or1 | btn_or2)
 * process_buttons()
 * Prev_state = Current state & btn_and1 & btn_and2
 *
 *
 * a click within a time interval is transmitted as btn_or = 1, btn_and = 0
 * It is desirable that the current state is true during a whole process interval.
 *
 *
 *
 * btn1or    0 0 1 0
 * btn1and   1 1 0 1
 * curstate  0 0 1 0
 * prevstate 0 0 0 0
 *               down_evt
 *                 no up_evt detectable from cur/prev!
 */

static void LEDTest(void) {
	LED_clear();

	static uint8_t val0;
	val0 += EncBuffer[0];
	static uint8_t val1;
	val1 += EncBuffer[1];

	LED_set(0, 0xffff >> (16 - (val0 >> 4)) );
	LED_set(1, 0xffff >> (16 - (val1 >> 4)) );

	static int button[16];
	IF_BTN_NAV_DOWN(btn_1) button[0] = ! button[0];
	IF_BTN_NAV_DOWN(btn_2) button[1] = ! button[1];
	IF_BTN_NAV_DOWN(btn_3) button[2] = ! button[2];
	IF_BTN_NAV_DOWN(btn_4) button[3] = ! button[3];
	IF_BTN_NAV_DOWN(btn_5) button[4] = ! button[4];
	IF_BTN_NAV_DOWN(btn_6) button[5] = ! button[5];
	IF_BTN_NAV_DOWN(btn_7) button[6] = ! button[6];
	IF_BTN_NAV_DOWN(btn_8) button[7] = ! button[7];
	IF_BTN_NAV_DOWN(btn_9) button[8] = ! button[8];
	IF_BTN_NAV_DOWN(btn_10) button[9] = ! button[9];
	IF_BTN_NAV_DOWN(btn_11) button[10] = ! button[10];
	IF_BTN_NAV_DOWN(btn_12) button[11] = ! button[11];
	IF_BTN_NAV_DOWN(btn_13) button[12] = ! button[12];
	IF_BTN_NAV_DOWN(btn_14) button[13] = ! button[13];
	IF_BTN_NAV_DOWN(btn_15) button[14] = ! button[14];
	IF_BTN_NAV_DOWN(btn_16) button[15] = ! button[15];

	int i=0;
	for(i=0;i<16;i++) {
		LED_setBit(2,i,button[i]);
	}
}

static void UIPollButtons(void) {
  Btn_Nav_CurStates.word = Btn_Nav_CurStates.word | Btn_Nav_Or.word;
  Btn_Nav_Or.word = 0;

  LEDTest();

  if (KvpsDisplay->kvptype == KVP_TYPE_AVP) {
    KeyValuePair_s *cur =
        &((KeyValuePair_s *)(KvpsDisplay->avp.array))[KvpsDisplay->avp.current];
    IF_BTN_NAV_DOWN(btn_nav_Down)
      KVP_Increment(KvpsDisplay);
    IF_BTN_NAV_DOWN(btn_nav_Up)
      KVP_Decrement(KvpsDisplay);
    IF_BTN_NAV_DOWN(btn_nav_Left)
      KVP_Decrement(cur);
    IF_BTN_NAV_DOWN(btn_nav_Right)
      KVP_Increment(cur);
    IF_BTN_NAV_DOWN(btn_nav_Enter) {
      if ((cur->kvptype == KVP_TYPE_AVP) || (cur->kvptype == KVP_TYPE_APVP)
          || (cur->kvptype == KVP_TYPE_CUSTOM))
        KvpsDisplay = cur;
      else if (cur->kvptype == KVP_TYPE_FNCTN)
        if (cur->fnctnvp.fnctn != 0)
          (cur->fnctnvp.fnctn)();
    }
    IF_BTN_NAV_DOWN(btn_nav_Back)
      if (KvpsDisplay->parent)
        KvpsDisplay = (KeyValuePair_s *)KvpsDisplay->parent;

  }
  else if (KvpsDisplay->kvptype == KVP_TYPE_APVP) {
    KeyValuePair_s *cur =
        (KeyValuePair_s *)(KvpsDisplay->apvp.array[KvpsDisplay->apvp.current]);
    IF_BTN_NAV_DOWN(btn_nav_Down)
      KVP_Increment(KvpsDisplay);
    IF_BTN_NAV_DOWN(btn_nav_Up)
      KVP_Decrement(KvpsDisplay);
    IF_BTN_NAV_DOWN(btn_nav_Left)
      KVP_Decrement(cur);
    IF_BTN_NAV_DOWN(btn_nav_Right)
      KVP_Increment(cur);
    IF_BTN_NAV_DOWN(btn_nav_Enter) {
      if ((cur->kvptype == KVP_TYPE_AVP) || (cur->kvptype == KVP_TYPE_APVP)
          || (cur->kvptype == KVP_TYPE_CUSTOM))
        KvpsDisplay = cur;
      else if (cur->kvptype == KVP_TYPE_FNCTN)
        if (cur->fnctnvp.fnctn != 0)
          (cur->fnctnvp.fnctn)();
    }
    IF_BTN_NAV_DOWN(btn_nav_Back)
      if (KvpsDisplay->parent)
        KvpsDisplay = (KeyValuePair_s *)KvpsDisplay->parent;

  }
  else if (KvpsDisplay->kvptype == KVP_TYPE_CUSTOM) {
    if (KvpsDisplay->custom.buttonFunction != 0) (KvpsDisplay->custom.buttonFunction)(KvpsDisplay->custom.userdata);

    IF_BTN_NAV_DOWN(btn_nav_Back)
      if (KvpsDisplay->parent)
        KvpsDisplay = (KeyValuePair_s *)KvpsDisplay->parent;

  }


// process encoder // todo: more than just one encoder...
  if (KvpsDisplay->kvptype == KVP_TYPE_AVP) {
    KeyValuePair_s *cur =
        &((KeyValuePair_s *)(KvpsDisplay->avp.array))[KvpsDisplay->avp.current];
    if ((cur->kvptype == KVP_TYPE_IVP) || (cur->kvptype == KVP_TYPE_IPVP)) {
      while (EncBuffer[0] > 0) {
        KVP_Increment(cur);
        EncBuffer[0]--;
      }
      while (EncBuffer[0] < 0) {
        KVP_Decrement(cur);
        EncBuffer[0]++;
      }
    }
  }
  else if (KvpsDisplay->kvptype == KVP_TYPE_APVP) {
    KeyValuePair_s *cur =
        (KeyValuePair_s *)(KvpsDisplay->apvp.array[KvpsDisplay->apvp.current]);
    if ((cur->kvptype == KVP_TYPE_IVP) || (cur->kvptype == KVP_TYPE_IPVP)) {
      while (EncBuffer[0] > 0) {
        KVP_Increment(cur);
        EncBuffer[0]--;
      }
      while (EncBuffer[0] < 0) {
        KVP_Decrement(cur);
        EncBuffer[0]++;
      }
    }
  }

  Btn_Nav_CurStates.word = Btn_Nav_CurStates.word & Btn_Nav_And.word;
  Btn_Nav_PrevStates = Btn_Nav_CurStates;
  Btn_Nav_And.word = ~0;


}

static void UIUpdateLCD(void) {
  KVP_DisplayInv(0, 0, KvpsDisplay);
  if (KvpsDisplay->kvptype == KVP_TYPE_AVP) {
    int c = KvpsDisplay->avp.current;
    int l = KvpsDisplay->avp.length;
    KeyValuePair_s *k = (KeyValuePair_s *)KvpsDisplay->avp.array;
    if (l < STATUSROW) {
      int i;
      for (i = 0; i < l; i++) {
        if (c == i)
          KVP_DisplayInv(LCD_COL_INDENT, 1 + i, &k[i]);
        else
          KVP_Display(LCD_COL_INDENT, 1 + i, &k[i]);
      }
      for (; i < STATUSROW; i++) {
        LCD_drawStringN(LCD_COL_INDENT, 1 + i, " ", 78);
      }
    }
    else if (c == 0) {
      if (c < l)
        KVP_DisplayInv(LCD_COL_INDENT, 1, &k[c++]);
      else
        LCD_drawStringN(LCD_COL_INDENT, 1, " ", LCDWIDTH);
      int line;
      for (line = 2; line < STATUSROW; line++) {
        if (c < l)
          KVP_Display(LCD_COL_INDENT, line, &k[c++]);
        else
          LCD_drawStringN(LCD_COL_INDENT, line, " ", LCDWIDTH);
      }
    }
    else if (c == 1) {
      c--;
      if (c < l)
        KVP_Display(LCD_COL_INDENT, 1, &k[c++]);
      else
        LCD_drawStringN(LCD_COL_INDENT, 1, " ", LCDWIDTH);
      if (c < l)
        KVP_DisplayInv(LCD_COL_INDENT, 2, &k[c++]);
      else
        LCD_drawStringN(LCD_COL_INDENT, 2, " ", LCDWIDTH);
      int line;
      for (line = 3; line < STATUSROW; line++) {
        if (c < l)
          KVP_Display(LCD_COL_INDENT, line, &k[c++]);
        else
          LCD_drawStringN(LCD_COL_INDENT, line, " ", LCDWIDTH);
      }
    }
    else {
      c--;
      c--;
      if (c < l)
        KVP_Display(LCD_COL_INDENT, 1, &k[c++]);
      else
        LCD_drawStringN(LCD_COL_INDENT, 1, " ", LCDWIDTH);
      if (c < l)
        KVP_Display(LCD_COL_INDENT, 2, &k[c++]);
      else
        LCD_drawStringN(LCD_COL_INDENT, 2, " ", LCDWIDTH);
      if (c < l)
        KVP_DisplayInv(LCD_COL_INDENT, 3, &k[c++]);
      else
        LCD_drawStringN(LCD_COL_INDENT, 3, " ", LCDWIDTH);
      int line;
      for (line = 3; line < STATUSROW; line++) {
        if (c < l)
          KVP_Display(LCD_COL_INDENT, line, &k[c++]);
        else
          LCD_drawStringN(LCD_COL_INDENT, line, " ", LCDWIDTH);
      }
    }
    if (KvpsDisplay->parent) {
      LCD_drawStringInv(0, STATUSROW, "BACK");
      LCD_drawString(24, STATUSROW, "     ");
    }
    else
      LCD_drawString(0, STATUSROW, "          ");
    if ((k[KvpsDisplay->avp.current].kvptype == KVP_TYPE_AVP)
        || (k[KvpsDisplay->avp.current].kvptype == KVP_TYPE_APVP)
        || (k[KvpsDisplay->avp.current].kvptype == KVP_TYPE_CUSTOM))
      LCD_drawStringInv(LCD_COL_ENTER, STATUSROW, "ENTER");
    else if (k[KvpsDisplay->avp.current].kvptype == KVP_TYPE_FNCTN)
      LCD_drawStringInv(LCD_COL_ENTER, STATUSROW, "ENTER");
    else
      LCD_drawString(LCD_COL_ENTER, STATUSROW, "     ");
  }
  else if (KvpsDisplay->kvptype == KVP_TYPE_APVP) {
    int c = KvpsDisplay->apvp.current;
    int l = KvpsDisplay->apvp.length;
    KeyValuePair_s **k = (KeyValuePair_s **)KvpsDisplay->apvp.array;
    if (l < 7) {
      int i;
      for (i = 0; i < l; i++) {
        if (c == i)
          KVP_DisplayInv(LCD_COL_INDENT, 1 + i, k[i]);
        else
          KVP_Display(LCD_COL_INDENT, 1 + i, k[i]);
      }
      for (; i < STATUSROW; i++) {
        LCD_drawStringN(LCD_COL_INDENT, 1 + i, " ", 78);
      }
    }
    else if (c == 0) {
      if (c < l)
        KVP_DisplayInv(LCD_COL_INDENT, 1, k[c++]);
      else
        LCD_drawStringN(LCD_COL_INDENT, 1, " ", LCDWIDTH);
      int line;
      for (line = 2; line < STATUSROW; line++) {
        if (c < l)
          KVP_Display(LCD_COL_INDENT, line, k[c++]);
        else
          LCD_drawStringN(LCD_COL_INDENT, line, " ", LCDWIDTH);
      }
    }
    else if (c == 1) {
      c--;
      if (c < l)
        KVP_Display(LCD_COL_INDENT, 1, k[c++]);
      else
        LCD_drawStringN(LCD_COL_INDENT, 1, " ", LCDWIDTH);
      if (c < l)
        KVP_DisplayInv(LCD_COL_INDENT, 2, k[c++]);
      else
        LCD_drawStringN(LCD_COL_INDENT, 2, " ", LCDWIDTH);
      int line;
      for (line = 3; line < STATUSROW; line++) {
        if (c < l)
          KVP_Display(LCD_COL_INDENT, line, k[c++]);
        else
          LCD_drawStringN(LCD_COL_INDENT, line, " ", LCDWIDTH);
      }
    }
    else {
      c--;
      c--;
      if (c < l)
        KVP_Display(LCD_COL_INDENT, 1, k[c++]);
      else
        LCD_drawStringN(LCD_COL_INDENT, 1, " ", LCDWIDTH);
      if (c < l)
        KVP_Display(LCD_COL_INDENT, 2, k[c++]);
      else
        LCD_drawStringN(LCD_COL_INDENT, 2, " ", LCDWIDTH);
      if (c < l)
        KVP_DisplayInv(LCD_COL_INDENT, 3, k[c++]);
      else
        LCD_drawStringN(LCD_COL_INDENT, 3, " ", LCDWIDTH);
      int line;
      for (line = 4; line < STATUSROW; line++) {
        if (c < l)
          KVP_Display(LCD_COL_INDENT, line, k[c++]);
        else
          LCD_drawStringN(LCD_COL_INDENT, line, " ", LCDWIDTH);
      }
    }
    if (KvpsDisplay->parent) {
      LCD_drawStringInv(0, STATUSROW, "BACK");
      LCD_drawString(24, STATUSROW, "     ");
    }
    else
      LCD_drawString(0, STATUSROW, "          ");
    if ((k[KvpsDisplay->apvp.current]->kvptype == KVP_TYPE_AVP)
        || (k[KvpsDisplay->apvp.current]->kvptype == KVP_TYPE_APVP)
        || (k[KvpsDisplay->apvp.current]->kvptype == KVP_TYPE_CUSTOM))
      LCD_drawStringInv(LCD_COL_ENTER, STATUSROW, "ENTER");
    else if (k[KvpsDisplay->avp.current]->kvptype == KVP_TYPE_FNCTN)
      LCD_drawStringInv(LCD_COL_ENTER, STATUSROW, "ENTER");
    else
      LCD_drawString(LCD_COL_ENTER, STATUSROW, "     ");
  }
  else if (KvpsDisplay->kvptype == KVP_TYPE_CUSTOM) {
	  if (KvpsDisplay->custom.displayFunction != 0) (KvpsDisplay->custom.displayFunction)(KvpsDisplay->custom.userdata);
  }
}


void k_scope_disp_frac32_64(void * userdata) {
// userdata  int32_t[64], one sample per column
  const int indent = (128 - (15 + 64)) / 2 ;
  int i;
  LCD_clearDisplay();
  for (i = 0; i < 48; i++) {
    LCD_setPixel(index + 14, i);
  }
  LCD_drawString(indent + 5, 0, "1");
  LCD_drawString(indent + 5, 2, "0");
  LCD_drawString(indent +0, 4, "-1");
  LCD_setPixel(indent + 13, 21);
  LCD_setPixel(indent + 12, 21);
  LCD_setPixel(indent + 13, 21 + 16);
  LCD_setPixel(indent + 12, 21 + 16);
  LCD_setPixel(indent + 13, 21 - 16);
  LCD_setPixel(indent + 12, 21 - 16);
  LCD_drawStringInv(0, STATUSROW, "BACK");
  LCD_drawStringInv(LCD_COL_LEFT, STATUSROW, "HOLD");
  for (i = 0; i < 64; i++) {
    int y = ((int *)userdata)[i];
    y = 21 - (y >> 23);
    if (y < 1)
      y = 1;
    if (y > 47)
      y = 47;
    LCD_setPixel(indent + i + 15, y);
  }
}

void k_scope_disp_frac32_minmax_64(void * userdata) {
// userdata  int32_t[64][2], minimum and maximum per column
  const int indent = (128 - (15 + 64)) / 2 ;
  int i;
  LCD_clearDisplay();
  for (i = 0; i < 48; i++) {
    LCD_setPixel(indent + 14, i);
  }
  LCD_drawString(indent + 5, 0, "1");
  LCD_drawString(indent + 5, 2, "0");
  LCD_drawString(indent + 0, 4, "-1");
  LCD_setPixel(indent + 13, 21);
  LCD_setPixel(indent + 12, 21);
  LCD_setPixel(indent + 13, 21 + 16);
  LCD_setPixel(indent + 12, 21 + 16);
  LCD_setPixel(indent + 13, 21 - 16);
  LCD_setPixel(indent + 12, 21 - 16);
  LCD_drawStringInv(0, STATUSROW, "BACK");
  LCD_drawStringInv(LCD_COL_LEFT, STATUSROW, "HOLD");

  for (i = 0; i < 64; i++) {
    int y = ((int *)userdata)[i * 2 + 1];
    y = 21 - (y >> 23);
    if (y < 1)
      y = 1;
    if (y > 47)
      y = 47;
    int y2 = ((int *)userdata)[i * 2];
    y2 = 21 - (y2 >> 23);
    if (y2 < 1)
      y2 = 1;
    if (y2 > 47)
      y2 = 47;
    int j;

    if (y2 <= (y))
      y2 = y + 1;
    for (j = y; j < y2; j++)
      LCD_setPixel(indent + i + 15, j);
  }
}

void k_scope_disp_frac32buffer_64(void * userdata) {
	k_scope_disp_frac32_64(userdata);
}




