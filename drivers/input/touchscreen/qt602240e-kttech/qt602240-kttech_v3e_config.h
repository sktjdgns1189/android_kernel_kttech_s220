
/**********************************************************
  DEVICE   : mxT224  0.2
  CUSTOMER : KT Tech
  PROJECT  : O3
  X SIZE   : X19
  Y SIZE   : Y11
  THRESHOLD : 0x20
***********************************************************/

#define __MXT224E_CONFIG__

/* Calibration Configuration Information */
#define T9_TCHTHR_CAL							30

/* SPT_USERDATA_T38 INSTANCE 0 */
#define T38_USERDATA0							0
#define T38_USERDATA1							7		/* CAL_THR */
#define T38_USERDATA2							13	/* num_of_antitouch */
#define T38_USERDATA3							10	/* touch skip size in calibration  */
#define T38_USERDATA4							0		/* MXT_ADR_T8_ATCHFRCCALRATIO for normal */
#define T38_USERDATA5							0
#define T38_USERDATA6							0
#define T38_USERDATA7							4		/* max touch for check_auto_cal */

/* GEN_POWERCONFIG_T7 */

#define T7_IDLEACQINT							15
#define T7_ACTVACQINT							16
#define T7_ACTV2IDLETO						50

/* _GEN_ACQUISITIONCONFIG_T8 INSTANCE 0 */
#define T8_CHRGTIME								30	/* 6 - 60  * 83 ns */
#define T8_ATCHDRIFT							0
#define T8_TCHDRIFT								5
#define T8_DRIFTST								5
#define T8_TCHAUTOCAL							0
#define T8_SYNC										0
#define T8_ATCHCALST							9
#define T8_ATCHCALSTHR						5
#define T8_ATCHFRCCALTHR					0		/* V2.0 added */
#define T8_ATCHFRCCALRATIO				0		/* V2.0 added */

/* TOUCH_MULTITOUCHSCREEN_T9 INSTANCE 0 */
#define T9_CTRL										139
#define T9_XORIGIN								0
#define T9_YORIGIN								0
#define T9_XSIZE									19
#define T9_YSIZE									11
#define T9_AKSCFG									0
#define T9_BLEN										32
#define T9_TCHTHR									50
#define T9_TCHDI									2
#define T9_ORIENT									1
#define T9_MRGTIMEOUT							0
#define T9_MOVHYSTI								10
#define T9_MOVHYSTN								2
#define T9_MOVFILTER							15
#define T9_NUMTOUCH								10
#define T9_MRGHYST								3
#define T9_MRGTHR									50
#define T9_AMPHYST								6
#define T9_XRANGE									1061
#define T9_YRANGE									539
#define T9_XLOCLIP								0
#define T9_XHICLIP								0
#define T9_YLOCLIP								0
#define T9_YHICLIP								0
#define T9_XEDGECTRL							0
#define T9_XEDGEDIST							0
#define T9_YEDGECTRL							0
#define T9_YEDGEDIST							0
#define T9_JUMPLIMIT							15
#define T9_TCHHYST								12	/* V2.0 or MXT224E added */
#define T9_XPITCH									0		/* MXT224E added */
#define T9_YPITCH									0		/* MXT224E added */
#define T9_NEXTTCHDI							0

/* TOUCH_KEYARRAY_T15 */
#define T15_CTRL									0 /* single key configuration*/  /* 0x03 = multi-key */
#define T15_XORIGIN								0
#define T15_YORIGIN								0
#define T15_XSIZE									0 
#define T15_YSIZE									0
#define T15_AKSCFG								0
#define T15_BLEN									0
#define T15_TCHTHR								0
#define T15_TCHDI									0
#define T15_RESERVED_0						0
#define T15_RESERVED_1						0

/* SPT_COMMSCONFIG_T18 */
#define T18_CTRL									0
#define T18_COMMAND								0

/* SPT_GPIOPWM_T19 INSTANCE 0 */
#define T19_CTRL									0
#define T19_REPORTMASK						0
#define T19_DIR										0
#define T19_INTPULLUP							0
#define T19_OUT										0
#define T19_WAKE									0
#define T19_PWM										0
#define T19_PERIOD								0
#define T19_DUTY_0								0
#define T19_DUTY_1								0
#define T19_DUTY_2								0
#define T19_DUTY_3								0
#define T19_TRIGGER_0							0
#define T19_TRIGGER_1							0
#define T19_TRIGGER_2							0
#define T19_TRIGGER_3							0


/* TOUCH_PROXIMITY_T23 */
#define T23_CTRL									0
#define T23_XORIGIN								0
#define T23_YORIGIN								0
#define T23_XSIZE									0
#define T23_YSIZE									0
#define T23_RESERVED							0
#define T23_BLEN									0
#define T23_FXDDTHR								0
#define T23_FXDDI									0
#define T23_AVERAGE								0
#define T23_MVNULLRATE						0
#define T23_MVDTHR								0

/* T24_[PROCI_ONETOUCHGESTUREPROCESSOR_T24 INSTANCE 0] */
#define T24_CTRL									0
#define T24_NUMGEST								0
#define T24_GESTEN								0
#define T24_PROCESS								0
#define T24_TAPTO									0
#define T24_FLICKTO								0
#define T24_DRAGTO								0
#define T24_SPRESSTO							0
#define T24_LPRESSTO							0
#define T24_REPPRESSTO						0
#define T24_FLICKTHR							0
#define T24_DRAGTHR								0
#define T24_TAPTHR								0
#define T24_THROWTHR							0


/* [SPT_SELFTEST_T25 INSTANCE 0] */
#define T25_CTRL									0
#define T25_CMD										0
#define T25_SIGLIM_0_UPSIGLIM			0
#define T25_SIGLIM_0_LOSIGLIM			0
#define T25_SIGLIM_1_UPSIGLIM			0
#define T25_SIGLIM_1_LOSIGLIM			0
#define T25_SIGLIM_2_UPSIGLIM			0
#define T25_SIGLIM_2_LOSIGLIM			0


/* PROCI_GRIPSUPPRESSION_T40 */

#define T40_CTRL									0
#define T40_XLOGRIP								0
#define T40_XHIGRIP								0
#define T40_YLOGRIP								0
#define T40_YHIGRIP								0

/* PROCI_TOUCHSUPPRESSION_T42 */
#define T42_CTRL									0
#define T42_APPRTHR								0		/* 0 (TCHTHR/4), 1 to 255 */
#define T42_MAXAPPRAREA						0		/* 0 (40ch), 1 to 255 */
#define T42_MAXTCHAREA						0		/* 0 (35ch), 1 to 255 */
#define T42_SUPSTRENGTH						0		/* 0 (128), 1 to 255 */
#define T42_SUPEXTTO							0		/* 0 (never expires), 1 to 255 (timeout in cycles) */
#define T42_MAXNUMTCHS						0		/* 0 to 9 (maximum number of touches minus 1) */
#define T42_SHAPESTRENGTH					0		/* 0 (10), 1 to 31 */

/* SPT_CTECONFIG_T46  */
#define T46_CTRL									4		/*Reserved */
#define T46_MODE									3		/*0: 16X14Y, 1: 17X13Y, 2: 18X12Y, 3: 19X11Y, 4: 20X10Y, 5: 21X15Y, 6: 22X8Y, */
#define T46_IDLESYNCSPERX					32
#define T46_ACTVSYNCSPERX					63
#define T46_ADCSPERSYNC						0
#define T46_PULSESPERADC					0		/*0:1  1:2   2:3   3:4 pulses */
#define T46_XSLEW									1		/*0:500nsec,  1:350nsec */
#define T46_SYNCDELAY							0

/* PROCI_STYLUS_T47 */
#define T47_CTRL									0
#define T47_CONTMIN								0
#define T47_CONTMAX								0
#define T47_STABILITY							0
#define T47_MAXTCHAREA						0
#define T47_AMPLTHR								0
#define T47_STYSHAPE							0
#define T47_HOVERSUP							0
#define T47_CONFTHR								0
#define T47_SYNCSPERX							0


/* PROCG_NOISESUPPRESSION_T48  */
/* for TA */
#define T48_CTRL_TA								19
#define T48_CFG_TA								0
#define T48_CALCFG_TA							112
#define T48_BASEFREQ_TA						0
#define	T48_RESERVED0_TA					0
#define	T48_RESERVED1_TA					0
#define	T48_RESERVED2_TA					0
#define	T48_RESERVED3_TA					0
#define T48_MFFREQ_2_TA						0
#define T48_MFFREQ_3_TA						0
#define	T48_RESERVED4_TA					0
#define	T48_RESERVED5_TA					0
#define	T48_RESERVED6_TA					0
#define T48_GCACTVINVLDADCS_TA		6
#define T48_GCIDLEINVLDADCS_TA		6
#define	T48_RESERVED7_TA					0
#define	T48_RESERVED8_TA					0
#define T48_GCMAXADCSPERX_TA			100
#define T48_GCLIMITMIN_TA					6
#define T48_GCLIMITMAX_TA					64
#define T48_GCCOUNTMINTGT_TA			10
#define T48_MFINVLDDIFFTHR_TA			32
#define T48_MFINCADCSPXTHR_TA			5
#define T48_MFERRORTHR_TA					38
#define	T48_SELFREQMAX_TA					10
#define	T48_RESERVED9_TA					0
#define	T48_RESERVED10_TA					0
#define	T48_RESERVED11_TA					0
#define	T48_RESERVED12_TA					0
#define	T48_RESERVED13_TA					0
#define	T48_RESERVED14_TA					0
#define T48_BLEN_TA								0
#define T48_TCHTHR_TA							50
#define T48_TCHDI_TA							2
#define T48_MOVHYSTI_TA						3
#define T48_MOVHYSTN_TA						1
#define	T48_MOVFILTER_TA					13
#define T48_NUMTOUCH_TA						10
#define T48_MRGHYST_TA						3
#define T48_MRGTHR_TA							50
#define T48_XLOCLIP_TA						0
#define T48_XHICLIP_TA						0
#define T48_YLOCLIP_TA						0
#define T48_YHICLIP_TA						0
#define T48_XEDGECTRL_TA					0
#define T48_XEDGEDIST_TA					0
#define T48_YEDGECTRL_TA					0
#define T48_YEDGEDIST_TA					0
#define T48_JUMPLIMIT_TA					15
#define T48_TCHHYST_TA						12
#define T48_NEXTTCHDI_TA					0

/* for BATTERY */
/* PROCG_NOISESUPPRESSION_T48  */
/* for TA */
#define T48_CTRL									19 
#define T48_CFG										0  
#define T48_CALCFG								96
#define T48_BASEFREQ							0  
#define	T48_RESERVED0							0  
#define	T48_RESERVED1							0  
#define	T48_RESERVED2							0  
#define	T48_RESERVED3							0  
#define T48_MFFREQ_2							0  
#define T48_MFFREQ_3							0  
#define	T48_RESERVED4							0  
#define	T48_RESERVED5							0  
#define	T48_RESERVED6							0  
#define T48_GCACTVINVLDADCS				6  
#define T48_GCIDLEINVLDADCS				6  
#define	T48_RESERVED7							0  
#define	T48_RESERVED8							0  
#define T48_GCMAXADCSPERX					100
#define T48_GCLIMITMIN						6  
#define T48_GCLIMITMAX						64 
#define T48_GCCOUNTMINTGT					10 
#define T48_MFINVLDDIFFTHR				32 
#define T48_MFINCADCSPXTHR				5  
#define T48_MFERRORTHR						38 
#define	T48_SELFREQMAX						10 
#define	T48_RESERVED9							0  
#define	T48_RESERVED10						0  
#define	T48_RESERVED11						0  
#define	T48_RESERVED12						0  
#define	T48_RESERVED13						0  
#define	T48_RESERVED14						0  
#define T48_BLEN									0  
#define T48_TCHTHR								50 
#define T48_TCHDI									2  
#define T48_MOVHYSTI							3  
#define T48_MOVHYSTN							1  
#define	T48_MOVFILTER							13 
#define T48_NUMTOUCH							10  
#define T48_MRGHYST								3  
#define T48_MRGTHR								50 
#define T48_XLOCLIP								0  
#define T48_XHICLIP								0  
#define T48_YLOCLIP								0  
#define T48_YHICLIP								0  
#define T48_XEDGECTRL							0  
#define T48_XEDGEDIST							0  
#define T48_YEDGECTRL							0  
#define T48_YEDGEDIST							0  
#define T48_JUMPLIMIT							15 
#define T48_TCHHYST								12 
#define T48_NEXTTCHDI							0  

#define	T48_CHGON_BIT							0x20
/********************* END  *********************/

