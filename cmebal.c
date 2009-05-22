/*
LADSPA plugin implementing a simple balance control (stereo input, stereo output).
CME 2007-10-05
*/


#include <stdlib.h>
#define _GNU_SOURCE
#include <string.h>
#include <math.h>

#include "ladspa.h"



#define CMEBALANCE_LADSPA_ID	52

#define CMEBALANCE_PORT_COUNT 5

/* The internal ID numbers for the plugin's ports: */

// Huh? You have to define these in numerical order?!  C is too low-level for this stuff, IMHO.
#define BALANCE_CONTROL	0
#define BALANCE_INPUT_L	1
#define BALANCE_INPUT_R	2
#define BALANCE_OUTPUT_L	3
#define BALANCE_OUTPUT_R	4





/* The structure used to hold port connection information and state
   (actually gain controls require no further state). */


typedef struct {
	LADSPA_Data * ControlValue;
	LADSPA_Data * LInputBuffer;
	LADSPA_Data * RInputBuffer;
	LADSPA_Data * LOutputBuffer;
	LADSPA_Data * ROutputBuffer;
} Balance;


/* Construct a new plugin instance. */
LADSPA_Handle 
instantiateBalance(const LADSPA_Descriptor * Descriptor,
		     unsigned long             SampleRate) {
	return malloc(sizeof(Balance));
}


/* Connect a port to a data location. */
void 
connectPortToBalance(LADSPA_Handle Instance,
		       unsigned long Port,
		       LADSPA_Data * DataLocation) {

	Balance * psBalance;

	psBalance = (Balance *)Instance;
	switch (Port) {
		case BALANCE_CONTROL:
			psBalance->ControlValue = DataLocation;
			break;
		case BALANCE_INPUT_L:
			psBalance->LInputBuffer = DataLocation;
			break;
		case BALANCE_INPUT_R:
			psBalance->RInputBuffer = DataLocation;
		case BALANCE_OUTPUT_L:
			psBalance->LOutputBuffer = DataLocation;
			break;
		case BALANCE_OUTPUT_R:
			psBalance->ROutputBuffer = DataLocation;
			break;
	}
}



void 
runBalance(LADSPA_Handle Instance,
		 unsigned long SampleCount) {
  
	LADSPA_Data * LInput;
	LADSPA_Data * RInput;
	LADSPA_Data * LOutput;
	LADSPA_Data * ROutput;
	LADSPA_Data BalanceValue;
	LADSPA_Data LGainFactor;
	LADSPA_Data RGainFactor;

	Balance * psBalance;
	unsigned long SampleIndex;

	psBalance = (Balance *)Instance;

	LInput = psBalance->LInputBuffer;
	RInput = psBalance->RInputBuffer;
	LOutput = psBalance->LOutputBuffer;
	ROutput = psBalance->ROutputBuffer;
	BalanceValue = *(psBalance->ControlValue);

	// Logarithmic gain functions, intersecting at (0, -3 dB):
	//LGainFactor = pow(10.0, 3 * (log2(1 - BalanceValue) - 1) / 20.0);
	//RGainFactor = pow(10.0, 3 * (log2(1 + BalanceValue) - 1) / 20.0);
	// On second thought, let's leave the y-intercept at 0 dB, so you don't get a reduction in volume when you engage the effect.  This means +3 dB boost at extreme settings, however, with risk of clipping.
	LGainFactor = pow(10.0, 3 * log2(1 - BalanceValue) / 20.0);
	RGainFactor = pow(10.0, 3 * log2(1 + BalanceValue) / 20.0);

	// Process the sample buffer:
	for (SampleIndex = 0; SampleIndex < SampleCount; SampleIndex++)
	{
			*(LOutput++) = *(LInput++) * LGainFactor;
			*(ROutput++) = *(RInput++) * RGainFactor;
	}
}






/* Throw away a simple delay line. */
void 
cleanupBalance(LADSPA_Handle Instance) {
	free(Instance);
}



LADSPA_Descriptor * g_psBalanceDescriptor = NULL;




void 
_init() {

	char ** pcPortNames;
	LADSPA_PortDescriptor * piPortDescriptors;
	LADSPA_PortRangeHint * psPortRangeHints;

	g_psBalanceDescriptor = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));

  
		g_psBalanceDescriptor->UniqueID = CMEBALANCE_LADSPA_ID;
		g_psBalanceDescriptor->Label = strdup("cme_pan");
		g_psBalanceDescriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
		g_psBalanceDescriptor->Name = strdup("Balance (CME)");
		g_psBalanceDescriptor->Maker = strdup("Chris Edwards");
		g_psBalanceDescriptor->Copyright = strdup("None");

		g_psBalanceDescriptor->PortCount = CMEBALANCE_PORT_COUNT;
		piPortDescriptors = (LADSPA_PortDescriptor *)calloc(CMEBALANCE_PORT_COUNT, sizeof(LADSPA_PortDescriptor));
		g_psBalanceDescriptor->PortDescriptors = (const LADSPA_PortDescriptor *)piPortDescriptors;
		piPortDescriptors[BALANCE_CONTROL] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
		piPortDescriptors[BALANCE_INPUT_L] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
		piPortDescriptors[BALANCE_INPUT_R] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
		piPortDescriptors[BALANCE_OUTPUT_L] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
		piPortDescriptors[BALANCE_OUTPUT_R] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
		
		pcPortNames = (char **)calloc(CMEBALANCE_PORT_COUNT, sizeof(char *));
		g_psBalanceDescriptor->PortNames = (const char **)pcPortNames;
		pcPortNames[BALANCE_CONTROL] = strdup("Balance");
		pcPortNames[BALANCE_INPUT_L] = strdup("Input (L)");
		pcPortNames[BALANCE_INPUT_L] = strdup("Input (R)");
		pcPortNames[BALANCE_OUTPUT_L] = strdup("Output (L)");
		pcPortNames[BALANCE_OUTPUT_R] = strdup("Output (R)");
		psPortRangeHints = ((LADSPA_PortRangeHint *) calloc(CMEBALANCE_PORT_COUNT, sizeof(LADSPA_PortRangeHint)));
		g_psBalanceDescriptor->PortRangeHints = (const LADSPA_PortRangeHint *)psPortRangeHints;

		psPortRangeHints[BALANCE_CONTROL].HintDescriptor = (
		    	LADSPA_HINT_BOUNDED_BELOW | 
			LADSPA_HINT_BOUNDED_ABOVE | 
			LADSPA_HINT_DEFAULT_0
		);
		psPortRangeHints[BALANCE_CONTROL].LowerBound = -1;
		psPortRangeHints[BALANCE_CONTROL].UpperBound = 1;

		psPortRangeHints[BALANCE_INPUT_L].HintDescriptor = 0;
		psPortRangeHints[BALANCE_INPUT_R].HintDescriptor = 0;

		psPortRangeHints[BALANCE_OUTPUT_L].HintDescriptor = 0;
		psPortRangeHints[BALANCE_OUTPUT_R].HintDescriptor = 0;

		g_psBalanceDescriptor->instantiate = instantiateBalance;
		g_psBalanceDescriptor->connect_port = connectPortToBalance;
		g_psBalanceDescriptor->activate = NULL;
		g_psBalanceDescriptor->run = runBalance;
		g_psBalanceDescriptor->run_adding = NULL;
		g_psBalanceDescriptor->set_run_adding_gain = NULL;
		g_psBalanceDescriptor->deactivate = NULL;
		g_psBalanceDescriptor->cleanup = cleanupBalance;


}


void
deleteDescriptor(LADSPA_Descriptor * psDescriptor) {
  unsigned long lIndex;
	if (psDescriptor) {
		free((char *)psDescriptor->Label);
		free((char *)psDescriptor->Name);
		free((char *)psDescriptor->Maker);
		free((char *)psDescriptor->Copyright);
		free((LADSPA_PortDescriptor *)psDescriptor->PortDescriptors);
		for (lIndex = 0; lIndex < psDescriptor->PortCount; lIndex++)
			free((char *)(psDescriptor->PortNames[lIndex]));
		free((char **)psDescriptor->PortNames);
		free((LADSPA_PortRangeHint *)psDescriptor->PortRangeHints);
		free(psDescriptor);
	}
}


void
_fini() {
	deleteDescriptor(g_psBalanceDescriptor);
}


/* Return a descriptor of the requested plugin type. There are two
   plugin types available in this library (pan and balance). */
const LADSPA_Descriptor * 
ladspa_descriptor(unsigned long Index) {
	/* Return the requested descriptor or null if the index is out of range. */
	switch (Index) {
	case 0:
		return g_psBalanceDescriptor;
	default:
		return NULL;
	}
}


