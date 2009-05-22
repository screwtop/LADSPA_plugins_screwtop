/*
LADSPA plugin implementing a simple pan control (mono input, stereo output).
CME 2007-10-05
*/


#include <stdlib.h>
#define _GNU_SOURCE
#include <string.h>
#include <math.h>

#include "ladspa.h"



#define CMEPAN_LADSPA_ID	51

#define CMEPAN_PORT_COUNT 4

/* The internal ID numbers for the plugin's ports: */

// Huh? You have to define these in numerical order?!  C is too low-level for this stuff, IMHO.
#define PAN_CONTROL	0
#define PAN_INPUT	1
#define PAN_OUTPUT_L	2
#define PAN_OUTPUT_R	3





/* The structure used to hold port connection information and state
   (actually gain controls require no further state). */


typedef struct {
	LADSPA_Data * ControlValue;
	LADSPA_Data * InputBuffer;
	LADSPA_Data * LOutputBuffer;
	LADSPA_Data * ROutputBuffer;
} Pan;


/* Construct a new plugin instance. */
LADSPA_Handle 
instantiatePan(const LADSPA_Descriptor * Descriptor,
		     unsigned long             SampleRate) {
	return malloc(sizeof(Pan));
}


/* Connect a port to a data location. */
void 
connectPortToPan(LADSPA_Handle Instance,
		       unsigned long Port,
		       LADSPA_Data * DataLocation) {

	Pan * psPan;

	psPan = (Pan *)Instance;
	switch (Port) {
		case PAN_CONTROL:
			psPan->ControlValue = DataLocation;
			break;
		case PAN_INPUT:
			psPan->InputBuffer = DataLocation;
			break;
		case PAN_OUTPUT_L:
			psPan->LOutputBuffer = DataLocation;
			break;
		case PAN_OUTPUT_R:
			psPan->ROutputBuffer = DataLocation;
			break;
	}
}



void 
runPan(LADSPA_Handle Instance,
		 unsigned long SampleCount) {
  
	LADSPA_Data * Input;
	LADSPA_Data * LOutput;
	LADSPA_Data * ROutput;
	LADSPA_Data PanValue;
	LADSPA_Data LGainFactor;	// Store it to reduce CPU load.
	LADSPA_Data RGainFactor;

	Pan * psPan;
	unsigned long SampleIndex;

	psPan = (Pan *)Instance;

	Input = psPan->InputBuffer;
	LOutput = psPan->LOutputBuffer;
	ROutput = psPan->ROutputBuffer;
	PanValue = *(psPan->ControlValue);

	// Logarithmic gain functions, intersecting at (0, 0 dB), and with +3 dB boost at extremes (pan = -1 and 1):
	LGainFactor = pow(10.0, 3 * log2(1 - PanValue) / 20.0);
	RGainFactor = pow(10.0, 3 * log2(1 + PanValue) / 20.0);

	// Process the sample buffer:
	for (SampleIndex = 0; SampleIndex < SampleCount; SampleIndex++)
	{
			*(LOutput++) = *(Input) * LGainFactor;
			*(ROutput++) = *(Input) * RGainFactor;
			Input++;
	}
}






/* Throw away a simple delay line. */
void 
cleanupPan(LADSPA_Handle Instance) {
	free(Instance);
}



LADSPA_Descriptor * g_psPanDescriptor = NULL;




void 
_init() {

	char ** pcPortNames;
	LADSPA_PortDescriptor * piPortDescriptors;
	LADSPA_PortRangeHint * psPortRangeHints;

	g_psPanDescriptor = (LADSPA_Descriptor *)malloc(sizeof(LADSPA_Descriptor));

  
		g_psPanDescriptor->UniqueID = CMEPAN_LADSPA_ID;
		g_psPanDescriptor->Label = strdup("cme_pan");
		g_psPanDescriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
		g_psPanDescriptor->Name = strdup("Pan (CME)");
		g_psPanDescriptor->Maker = strdup("Chris Edwards");
		g_psPanDescriptor->Copyright = strdup("None");

		g_psPanDescriptor->PortCount = CMEPAN_PORT_COUNT;
		piPortDescriptors = (LADSPA_PortDescriptor *)calloc(CMEPAN_PORT_COUNT, sizeof(LADSPA_PortDescriptor));
		g_psPanDescriptor->PortDescriptors = (const LADSPA_PortDescriptor *)piPortDescriptors;
		piPortDescriptors[PAN_CONTROL] = LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL;
		piPortDescriptors[PAN_INPUT] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
		piPortDescriptors[PAN_OUTPUT_L] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
		piPortDescriptors[PAN_OUTPUT_R] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
		
		pcPortNames = (char **)calloc(CMEPAN_PORT_COUNT, sizeof(char *));
		g_psPanDescriptor->PortNames = (const char **)pcPortNames;
		pcPortNames[PAN_CONTROL] = strdup("Pan");
		pcPortNames[PAN_INPUT] = strdup("Input");
		pcPortNames[PAN_OUTPUT_L] = strdup("Output (L)");
		pcPortNames[PAN_OUTPUT_R] = strdup("Output (R)");
		psPortRangeHints = ((LADSPA_PortRangeHint *) calloc(CMEPAN_PORT_COUNT, sizeof(LADSPA_PortRangeHint)));
		g_psPanDescriptor->PortRangeHints = (const LADSPA_PortRangeHint *)psPortRangeHints;

		psPortRangeHints[PAN_CONTROL].HintDescriptor = (
		    	LADSPA_HINT_BOUNDED_BELOW | 
			LADSPA_HINT_BOUNDED_ABOVE | 
			LADSPA_HINT_DEFAULT_0
		);
		psPortRangeHints[PAN_CONTROL].LowerBound = -1;
		psPortRangeHints[PAN_CONTROL].UpperBound = 1;

		psPortRangeHints[PAN_INPUT].HintDescriptor = 0;
		psPortRangeHints[PAN_OUTPUT_L].HintDescriptor = 0;
		psPortRangeHints[PAN_OUTPUT_R].HintDescriptor = 0;

		g_psPanDescriptor->instantiate = instantiatePan;
		g_psPanDescriptor->connect_port = connectPortToPan;
		g_psPanDescriptor->activate = NULL;
		g_psPanDescriptor->run = runPan;
		g_psPanDescriptor->run_adding = NULL;
		g_psPanDescriptor->set_run_adding_gain = NULL;
		g_psPanDescriptor->deactivate = NULL;
		g_psPanDescriptor->cleanup = cleanupPan;


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
	deleteDescriptor(g_psPanDescriptor);
}


/* Return a descriptor of the requested plugin type. There are two
   plugin types available in this library (pan and balance). */
const LADSPA_Descriptor * 
ladspa_descriptor(unsigned long Index) {
	/* Return the requested descriptor or null if the index is out of range. */
	switch (Index) {
	case 0:
		return g_psPanDescriptor;
	default:
		return NULL;
	}
}


