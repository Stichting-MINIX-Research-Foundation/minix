/* Generated automatically from machmode.def and config/arm/arm-modes.def
   by genmodes.  */

#ifndef GCC_INSN_MODES_H
#define GCC_INSN_MODES_H

enum machine_mode
{
  VOIDmode,                /* machmode.def:169 */
  BLKmode,                 /* machmode.def:173 */
  CCmode,                  /* machmode.def:201 */
  CC_NOOVmode,             /* config/arm/arm-modes.def:40 */
  CC_Zmode,                /* config/arm/arm-modes.def:41 */
  CC_SWPmode,              /* config/arm/arm-modes.def:42 */
  CCFPmode,                /* config/arm/arm-modes.def:43 */
  CCFPEmode,               /* config/arm/arm-modes.def:44 */
  CC_DNEmode,              /* config/arm/arm-modes.def:45 */
  CC_DEQmode,              /* config/arm/arm-modes.def:46 */
  CC_DLEmode,              /* config/arm/arm-modes.def:47 */
  CC_DLTmode,              /* config/arm/arm-modes.def:48 */
  CC_DGEmode,              /* config/arm/arm-modes.def:49 */
  CC_DGTmode,              /* config/arm/arm-modes.def:50 */
  CC_DLEUmode,             /* config/arm/arm-modes.def:51 */
  CC_DLTUmode,             /* config/arm/arm-modes.def:52 */
  CC_DGEUmode,             /* config/arm/arm-modes.def:53 */
  CC_DGTUmode,             /* config/arm/arm-modes.def:54 */
  CC_Cmode,                /* config/arm/arm-modes.def:55 */
  CC_Nmode,                /* config/arm/arm-modes.def:56 */
  BImode,                  /* machmode.def:176 */
  QImode,                  /* machmode.def:181 */
  HImode,                  /* machmode.def:182 */
  SImode,                  /* machmode.def:183 */
  DImode,                  /* machmode.def:184 */
  TImode,                  /* machmode.def:185 */
  EImode,                  /* config/arm/arm-modes.def:67 */
  OImode,                  /* config/arm/arm-modes.def:68 */
  CImode,                  /* config/arm/arm-modes.def:69 */
  XImode,                  /* config/arm/arm-modes.def:70 */
  QQmode,                  /* machmode.def:204 */
  HQmode,                  /* machmode.def:205 */
  SQmode,                  /* machmode.def:206 */
  DQmode,                  /* machmode.def:207 */
  TQmode,                  /* machmode.def:208 */
  UQQmode,                 /* machmode.def:210 */
  UHQmode,                 /* machmode.def:211 */
  USQmode,                 /* machmode.def:212 */
  UDQmode,                 /* machmode.def:213 */
  UTQmode,                 /* machmode.def:214 */
  HAmode,                  /* machmode.def:216 */
  SAmode,                  /* machmode.def:217 */
  DAmode,                  /* machmode.def:218 */
  TAmode,                  /* machmode.def:219 */
  UHAmode,                 /* machmode.def:221 */
  USAmode,                 /* machmode.def:222 */
  UDAmode,                 /* machmode.def:223 */
  UTAmode,                 /* machmode.def:224 */
  HFmode,                  /* config/arm/arm-modes.def:29 */
  SFmode,                  /* machmode.def:196 */
  DFmode,                  /* machmode.def:197 */
  XFmode,                  /* config/arm/arm-modes.def:26 */
  SDmode,                  /* machmode.def:236 */
  DDmode,                  /* machmode.def:237 */
  TDmode,                  /* machmode.def:238 */
  CQImode,                 /* machmode.def:232 */
  CHImode,                 /* machmode.def:232 */
  CSImode,                 /* machmode.def:232 */
  CDImode,                 /* machmode.def:232 */
  CTImode,                 /* machmode.def:232 */
  CEImode,                 /* machmode.def:232 */
  COImode,                 /* machmode.def:232 */
  CCImode,                 /* machmode.def:232 */
  CXImode,                 /* machmode.def:232 */
  HCmode,                  /* machmode.def:233 */
  SCmode,                  /* machmode.def:233 */
  DCmode,                  /* machmode.def:233 */
  XCmode,                  /* machmode.def:233 */
  V4QImode,                /* config/arm/arm-modes.def:59 */
  V2HImode,                /* config/arm/arm-modes.def:59 */
  V8QImode,                /* config/arm/arm-modes.def:60 */
  V4HImode,                /* config/arm/arm-modes.def:60 */
  V2SImode,                /* config/arm/arm-modes.def:60 */
  V16QImode,               /* config/arm/arm-modes.def:61 */
  V8HImode,                /* config/arm/arm-modes.def:61 */
  V4SImode,                /* config/arm/arm-modes.def:61 */
  V2DImode,                /* config/arm/arm-modes.def:61 */
  V4HFmode,                /* config/arm/arm-modes.def:62 */
  V2SFmode,                /* config/arm/arm-modes.def:62 */
  V8HFmode,                /* config/arm/arm-modes.def:63 */
  V4SFmode,                /* config/arm/arm-modes.def:63 */
  V2DFmode,                /* config/arm/arm-modes.def:63 */
  MAX_MACHINE_MODE,

  MIN_MODE_RANDOM = VOIDmode,
  MAX_MODE_RANDOM = BLKmode,

  MIN_MODE_CC = CCmode,
  MAX_MODE_CC = CC_Nmode,

  MIN_MODE_INT = QImode,
  MAX_MODE_INT = XImode,

  MIN_MODE_PARTIAL_INT = VOIDmode,
  MAX_MODE_PARTIAL_INT = VOIDmode,

  MIN_MODE_FRACT = QQmode,
  MAX_MODE_FRACT = TQmode,

  MIN_MODE_UFRACT = UQQmode,
  MAX_MODE_UFRACT = UTQmode,

  MIN_MODE_ACCUM = HAmode,
  MAX_MODE_ACCUM = TAmode,

  MIN_MODE_UACCUM = UHAmode,
  MAX_MODE_UACCUM = UTAmode,

  MIN_MODE_FLOAT = HFmode,
  MAX_MODE_FLOAT = XFmode,

  MIN_MODE_DECIMAL_FLOAT = SDmode,
  MAX_MODE_DECIMAL_FLOAT = TDmode,

  MIN_MODE_COMPLEX_INT = CQImode,
  MAX_MODE_COMPLEX_INT = CXImode,

  MIN_MODE_COMPLEX_FLOAT = HCmode,
  MAX_MODE_COMPLEX_FLOAT = XCmode,

  MIN_MODE_VECTOR_INT = V4QImode,
  MAX_MODE_VECTOR_INT = V2DImode,

  MIN_MODE_VECTOR_FRACT = VOIDmode,
  MAX_MODE_VECTOR_FRACT = VOIDmode,

  MIN_MODE_VECTOR_UFRACT = VOIDmode,
  MAX_MODE_VECTOR_UFRACT = VOIDmode,

  MIN_MODE_VECTOR_ACCUM = VOIDmode,
  MAX_MODE_VECTOR_ACCUM = VOIDmode,

  MIN_MODE_VECTOR_UACCUM = VOIDmode,
  MAX_MODE_VECTOR_UACCUM = VOIDmode,

  MIN_MODE_VECTOR_FLOAT = V4HFmode,
  MAX_MODE_VECTOR_FLOAT = V2DFmode,

  NUM_MACHINE_MODES = MAX_MACHINE_MODE
};

#define CONST_MODE_SIZE const
#define CONST_MODE_BASE_ALIGN const
#define CONST_MODE_IBIT const
#define CONST_MODE_FBIT const

#endif /* insn-modes.h */
