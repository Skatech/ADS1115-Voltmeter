#define SIMULATION

#ifdef SIMULATION
#define USE_ADS1015     // only available in Proteus
#define TITLE_MESSAGE "ADS1015 VOLTMETER"
#else
#define USE_ADS1115
#define TITLE_MESSAGE "ADS1115 VOLTMETER"
#endif
